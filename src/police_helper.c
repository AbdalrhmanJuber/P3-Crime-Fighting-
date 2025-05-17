#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "../include/police.h"
#include "../include/ipc.h"
#include "../include/utils.h"
#include "../include/config.h"

// Function to process all intelligence reports from the queue
void process_intelligence_reports(Police* police, SimulationConfig config) {
    // Get the report queue ID
    int report_queue_id = msgget(REPORT_QUEUE_KEY, 0666);
    if (report_queue_id == -1) {
        perror("Police: Failed to get report queue");
        return;
    }
    
    // Process all available reports
    IntelligenceReport report;
    while (receive_report(report_queue_id, &report) > 0) {
        // Process each intelligence report
        process_intelligence(police, report, config);
    }
}

// Function to decide and take action against gangs
void take_police_action(Police* police, SimulationConfig config, 
                        SharedState* shm, int sem_id) {
    // Iterate through our suspicion record
    for (int i = 0; i < MAX_GANGS; i++) {
        // Check if we have enough suspicion to act
        if (police->suspicion_level[i] >= config.suspicion_threshold &&
            police->reliable_reports[i] >= config.min_reliable_reports) {
            
            // Take action against this gang
            log_message("Police: Taking action against Gang %d (Suspicion: %d)", 
                       i, police->suspicion_level[i]);
            
            // Arrest gang members
            int arrested = arrest_gang_members(police, i, config);
            
            if (arrested) {
                // Update global counters in shared memory
                semaphore_wait(sem_id, 0);
                shm->total_thwarted_missions++;
                shm->gang_status[i].is_arrested = true;
                shm->gang_status[i].prison_time = config.prison_time;
                semaphore_signal(sem_id, 0);
                
                // Reset suspicion for this gang
                police->suspicion_level[i] = 0;
                police->reliable_reports[i] = 0;
            }
        }
    }
    
    // Update lost agents counter in shared memory
    semaphore_wait(sem_id, 0);
    shm->total_executed_agents = police->lost_agents;
    semaphore_signal(sem_id, 0);
}
