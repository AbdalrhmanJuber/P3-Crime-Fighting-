#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/police.h"
#include "../include/utils.h"
#include "../include/ipc.h"

// Initialize police
void initialize_police(Police* police, SimulationConfig config) {
    // Initialize report storage
    police->report_capacity = 100;
    police->reports = (IntelligenceReport*)malloc(police->report_capacity * sizeof(IntelligenceReport));
    police->num_reports = 0;
    
    // Initialize statistics
    police->thwarted_missions = 0;
    police->total_agents = 0;
    police->lost_agents = 0;
    
    // Initialize synchronization
    pthread_mutex_init(&police->police_mutex, NULL);
    pthread_cond_init(&police->police_cond, NULL);
    
    log_message("Police force initialized");
}

// Process intelligence report
void process_intelligence(Police* police, IntelligenceReport report, SimulationConfig config) {
    pthread_mutex_lock(&police->police_mutex);
    
    // Log report receipt
    log_message("Police received intelligence from agent %d in gang %d (Suspicion: %d, Reliable: %s, Target: %s)",
                report.agent_id, report.gang_id, report.suspicion_level,
                report.is_reliable ? "Yes" : "No", 
                crime_type_to_string(report.suspected_target));
    
    // Store the report
    if (police->num_reports < police->report_capacity) {
        police->reports[police->num_reports++] = report;
    }
    else {
        // Expand capacity if needed
        police->report_capacity *= 2;
        police->reports = (IntelligenceReport*)realloc(police->reports, 
                                                      police->report_capacity * sizeof(IntelligenceReport));
        police->reports[police->num_reports++] = report;
    }
    
    // Check if immediate action is needed for high-risk crimes
    if (report.suspicion_level > config.police_action_threshold && report.is_reliable) {
        switch (report.suspected_target) {
            case KIDNAPPING:
            case BANK_ROBBERY:
            case ARM_TRAFFICKING:
                log_message("Police prioritizing response to high-risk crime: %s by gang %d", 
                           crime_type_to_string(report.suspected_target), report.gang_id);
                // Immediate decision could be made here
                break;
            default:
                // Normal processing for other crime types
                break;
        }
    }
    
    pthread_mutex_unlock(&police->police_mutex);
}

// Decide whether to take action based on intelligence
bool decide_on_action(Police* police, int gang_id, SimulationConfig config) {
    pthread_mutex_lock(&police->police_mutex);
    
    int total_suspicion = 0;
    int num_reports_for_gang = 0;
    int num_reliable_reports = 0;
    CrimeType suspected_crimes[7] = {0}; // Tracking different crime types reported
    
    // Analyze reports for the specified gang
    for (int i = 0; i < police->num_reports; i++) {
        if (police->reports[i].gang_id == gang_id) {
            total_suspicion += police->reports[i].suspicion_level;
            num_reports_for_gang++;
            
            // Track crime types reported
            suspected_crimes[police->reports[i].suspected_target]++;
            
            if (police->reports[i].is_reliable) {
                num_reliable_reports++;
            }
        }
    }
    
    // Calculate average suspicion level
    int avg_suspicion = 0;
    if (num_reports_for_gang > 0) {
        avg_suspicion = total_suspicion / num_reports_for_gang;
    }
    
    // Find most reported crime type
    int max_reports = 0;
    CrimeType most_likely_crime = BANK_ROBBERY; // Default
    for (int i = 0; i < NUM_CRIME_TYPES; i++) {
        if (suspected_crimes[i] > max_reports) {
            max_reports = suspected_crimes[i];
            most_likely_crime = (CrimeType)i;
        }
    }
    
    // Decision logic: take action if average suspicion is above threshold
    // and there is at least one reliable report
    bool decision = (avg_suspicion >= config.police_action_threshold && num_reliable_reports > 0);
    
    if (decision) {
        log_message("Police decided to take action against gang %d (Avg suspicion: %d, Reliable reports: %d, Suspected crime: %s)",
                    gang_id, avg_suspicion, num_reliable_reports, crime_type_to_string(most_likely_crime));
    }
    
    pthread_mutex_unlock(&police->police_mutex);
    
    return decision;
}

// Arrest gang members
void arrest_gang_members(Police* police, int gang_id, SimulationConfig config) {
    // Get shared memory to communicate with the gang process
    int shm_id = shmget(SHARED_MEMORY_KEY, 0, 0);
    if (shm_id == -1) {
        perror("Failed to find shared memory for arrest");
        return;
    }
    
    SharedState* shm = attach_shared_memory(shm_id);
    
    // Set the gang's prison time - random value between min and max from config
    int prison_time = random_int(config.prison_time_min, config.prison_time_max);
    
    // Get the semaphore ID - try to find it the same way we found the shared memory
    int sem_id = -1;
    for (int i = 0; i < 100; i++) {  // Try some IDs to find the semaphore
        sem_id = semget(SEMAPHORE_KEY, 0, 0);
        if (sem_id != -1) break;
    }
    
    if (sem_id == -1) {
        perror("Failed to find semaphore for arrest");
        detach_shared_memory(shm);
        return;
    }
    
    // Update the gang status in shared memory
    semaphore_wait(sem_id, 0);  // Get exclusive access
    
    if (gang_id < shm->num_gangs) {
        // Set arrest status
        shm->gang_status[gang_id].is_arrested = true;
        shm->gang_status[gang_id].prison_time = prison_time;
        shm->gang_status[gang_id].arrest_notification_seen = false;
        
        log_message("Police arrested members of gang %d for %d time units", gang_id, prison_time);
    }
    
    semaphore_signal(sem_id, 0);  // Release exclusive access
    
    // Update statistics
    pthread_mutex_lock(&police->police_mutex);
    police->thwarted_missions++;
    pthread_mutex_unlock(&police->police_mutex);
    
    // Detach from shared memory
    detach_shared_memory(shm);
}

// Police routine (background thread)
void* police_routine(void* arg) {
    Police* police = (Police*)arg;
    
    // Main police monitoring loop
    while (1) {
        pthread_mutex_lock(&police->police_mutex);
        
        // Analyze all reports to identify patterns
        int reports_by_gang[100] = {0};  // Count reports by gang ID (assumes max 100 gangs)
        int max_reports = 0;
        int max_gang_id = -1;
        
        for (int i = 0; i < police->num_reports; i++) {
            int gang_id = police->reports[i].gang_id;
            reports_by_gang[gang_id]++;
            
            if (reports_by_gang[gang_id] > max_reports) {
                max_reports = reports_by_gang[gang_id];
                max_gang_id = gang_id;
            }
        }
        
        // Log police activity periodically
        if (max_gang_id >= 0 && max_reports > 2) {
            log_message("Police monitoring gang %d closely (%d reports received)", 
                       max_gang_id, max_reports);
        }
        
        pthread_mutex_unlock(&police->police_mutex);
        
        // Sleep to avoid busy waiting
        sleep(2);
    }
    
    return NULL;
}

// Clean up police resources
void cleanup_police(Police* police) {
    // Destroy mutex and condition variable
    pthread_mutex_destroy(&police->police_mutex);
    pthread_cond_destroy(&police->police_cond);
    
    // Free allocated memory
    free(police->reports);
    
    log_message("Police resources cleaned up");
}
