#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/gang.h"
#include "../include/utils.h"
#include "../include/ipc.h"

// Initialize a gang
void initialize_gang(Gang* gang, int id, int num_members, int num_ranks, SimulationConfig config) {
    gang->id = id;
    gang->num_members = num_members;
    gang->num_ranks = num_ranks;
    gang->is_active = true;
    gang->is_in_prison = false;
    gang->prison_time_remaining = 0;
    gang->successful_missions = 0;
    gang->thwarted_missions = 0;
    gang->executed_agents = 0;
    gang->false_info_probability = config.false_info_probability;
    gang->report_queue_id = -1; // Will be set by the main process
    
    // Initialize mutex and condition variable
    pthread_mutex_init(&gang->gang_mutex, NULL);
    pthread_cond_init(&gang->gang_cond, NULL);
    
    // Allocate members
    gang->members = (GangMember*)malloc(num_members * sizeof(GangMember));
    
    // Initialize gang members
    for (int i = 0; i < num_members; i++) {
        gang->members[i].id = i;
        gang->members[i].rank = i % num_ranks;  // Distribute ranks evenly at first
        gang->members[i].preparation_level = 0;
        gang->members[i].knowledge_rate = 0;
        gang->members[i].gang_ptr = gang;
        
        // Determine if this member is a secret agent
        gang->members[i].is_secret_agent = random_event(config.agent_infiltration_success_rate);
    }
    
    // Store process ID
    gang->pid = getpid();
    
    // Plan initial mission
    plan_new_mission(gang, config);
    
    // Create threads for gang members
    for (int i = 0; i < num_members; i++) {
        pthread_create(&gang->members[i].thread, NULL, gang_member_routine, &gang->members[i]);
    }
    
    log_message("Gang %d initialized with %d members and %d ranks", id, num_members, num_ranks);
}

// Gang member thread routine
void* gang_member_routine(void* arg) {
    GangMember* member = (GangMember*)arg;
    Gang* gang = (Gang*)member->gang_ptr;
    
    while (gang->is_active) {
        // Wait if gang is in prison
        pthread_mutex_lock(&gang->gang_mutex);
        while (gang->is_in_prison) {
            pthread_cond_wait(&gang->gang_cond, &gang->gang_mutex);
        }
        pthread_mutex_unlock(&gang->gang_mutex);
        
        // Increase preparation level
        pthread_mutex_lock(&gang->gang_mutex);
        if (member->preparation_level < gang->required_preparation_level) {
            // Higher rank members prepare faster
            int preparation_step = 5 + (member->rank * 2); // Increased step size to make progress visible
            member->preparation_level += preparation_step;
            
            if (member->preparation_level > gang->required_preparation_level) {
                member->preparation_level = gang->required_preparation_level;
            }
            
            // If member is a secret agent, gather intelligence
            if (member->is_secret_agent) {
                // Update knowledge rate based on interactions with other members
                // Higher rank members have more accurate information
                int base_knowledge_increase = 5 + (member->rank * 3);
                
                // False information might decrease knowledge
                if (random_event(gang->false_info_probability)) {
                    member->knowledge_rate -= base_knowledge_increase / 2;
                    if (member->knowledge_rate < 0) {
                        member->knowledge_rate = 0;
                    }
                } else {
                    member->knowledge_rate += base_knowledge_increase;
                    if (member->knowledge_rate > 100) {
                        member->knowledge_rate = 100;
                    }
                }                    // Report to police if suspicion is high enough
                    if (member->knowledge_rate >= gang->required_preparation_level / 2) {
                        // Create intelligence report
                        IntelligenceReport report;
                        report.gang_id = gang->id;
                        report.agent_id = member->id;
                        report.suspected_target = gang->current_target;
                        report.suspicion_level = member->knowledge_rate;
                        report.is_reliable = member->rank > (gang->num_ranks / 2);
                        
                        // Submit report to police through message queue
                        int report_queue_id = gang->report_queue_id;
                        if (report_queue_id > 0) {
                            if (send_report(report_queue_id, report) == 0) {
                                log_message("Agent %d in gang %d submitted a report with suspicion level %d", 
                                           member->id, gang->id, member->knowledge_rate);
                            } else {
                                // If sending fails, we'll retry later
                                log_message("Agent %d in gang %d failed to submit report - will retry later", 
                                           member->id, gang->id);
                            }
                        }
                    }
            }
        }
        pthread_mutex_unlock(&gang->gang_mutex);
        
        // Sleep to avoid busy waiting
        usleep(500000); // 0.5 seconds between actions
    }
    
    return NULL;
}

// Plan a new mission for the gang
void plan_new_mission(Gang* gang, SimulationConfig config) {
    pthread_mutex_lock(&gang->gang_mutex);
    
    // Reset preparation levels
    for (int i = 0; i < gang->num_members; i++) {
        gang->members[i].preparation_level = 0;
    }
    
    // Select a random target
    gang->current_target = (CrimeType)random_int(0, NUM_CRIME_TYPES - 1);
    
    // Set preparation time
    gang->preparation_time = random_int(config.preparation_time_min, config.preparation_time_max);
    
    // Set required preparation level
    gang->required_preparation_level = random_int(config.min_preparation_level, config.max_preparation_level);
    
    log_message("Gang %d planning new mission: %s (Prep time: %d, Required level: %d)", 
                gang->id, crime_type_to_string(gang->current_target), 
                gang->preparation_time, gang->required_preparation_level);
    
    pthread_mutex_unlock(&gang->gang_mutex);
}

// Execute the mission
void execute_mission(Gang* gang, SimulationConfig config) {
    // Check if all members are prepared
    pthread_mutex_lock(&gang->gang_mutex);
    
    int total_preparation = 0;
    int max_possible_prep = 0;
    for (int i = 0; i < gang->num_members; i++) {
        total_preparation += gang->members[i].preparation_level;
        max_possible_prep += gang->required_preparation_level;
    }
    // Calculate as percentage of required level
    int average_preparation = max_possible_prep > 0 ? (total_preparation * 100) / max_possible_prep : 0;
    
    // Determine if mission is successful
    // Success rate increases with preparation level and preparation time
    int success_bonus = (average_preparation * gang->preparation_time) / 100;
    int success_chance = config.mission_success_rate_base + success_bonus;
    if (success_chance > 95) success_chance = 95; // Cap at 95%
    
    bool mission_success = random_event(success_chance);
    
    log_message("Gang %d attempting to execute mission: %s (Avg prep: %d%%, Success chance: %d%%)", 
                gang->id, crime_type_to_string(gang->current_target), 
                average_preparation, success_chance);
    
    if (mission_success) {
        gang->successful_missions++;
        log_message("Gang %d successfully executed mission: %s", 
                    gang->id, crime_type_to_string(gang->current_target));
        
        // Check for member deaths during mission
        for (int i = 0; i < gang->num_members; i++) {
            if (random_event(config.member_death_probability)) {
                log_message("Gang %d member %d died during mission", gang->id, gang->members[i].id);
                
                // Replace the dead member with a new one
                gang->members[i].rank = 0;  // Lowest rank
                gang->members[i].preparation_level = 0;
                gang->members[i].knowledge_rate = 0;
                
                // Determine if new member is a secret agent
                gang->members[i].is_secret_agent = random_event(config.agent_infiltration_success_rate);
            }
        }
    }
    else {
        gang->thwarted_missions++;
        log_message("Gang %d failed to execute mission: %s", 
                    gang->id, crime_type_to_string(gang->current_target));
        
        // Investigate for secret agents if they fail too many times
        if (gang->thwarted_missions % 2 == 0) {
            investigate_for_agents(gang, config);
        }
    }
    
    pthread_mutex_unlock(&gang->gang_mutex);
}

// Investigate for secret agents
void investigate_for_agents(Gang* gang, SimulationConfig config) {
    pthread_mutex_lock(&gang->gang_mutex);
    
    log_message("Gang %d starting internal investigation", gang->id);
    
    // Factors in investigation
    typedef struct {
        int member_id;
        int suspicion_score;
        bool is_agent;
    } SuspiciousAgent;
    
    SuspiciousAgent* suspects = (SuspiciousAgent*)malloc(gang->num_members * sizeof(SuspiciousAgent));
    int num_suspects = 0;
    
    // First phase: Calculate suspicion scores based on multiple factors
    for (int i = 0; i < gang->num_members; i++) {
        GangMember* member = &gang->members[i];
        
        // Base suspicion score
        int suspicion_score = 0;
        
        // 1. Low preparation level may indicate lack of commitment
        if (member->preparation_level < (gang->required_preparation_level / 2)) {
            suspicion_score += 20;
        }
        
        // 2. Newer members (lower ranks) are more suspicious
        suspicion_score += (gang->num_ranks - member->rank) * 5;
        
        // 3. Check for suspicious behavior patterns (could be expanded)
        if (member->knowledge_rate > 80 && member->rank < 2) {
            // Low-rank members shouldn't know too much
            suspicion_score += 25;
        }
        
        // If suspicion score is high enough, add to list of suspects
        if (suspicion_score > 30) {
            suspects[num_suspects].member_id = i;
            suspects[num_suspects].suspicion_score = suspicion_score;
            suspects[num_suspects].is_agent = member->is_secret_agent;
            num_suspects++;
        }
    }
    
    log_message("Gang %d identified %d suspicious members", gang->id, num_suspects);
    
    // Second phase: Interrogate suspects starting with most suspicious
    // Sort suspects by suspicion score (simple bubble sort)
    for (int i = 0; i < num_suspects - 1; i++) {
        for (int j = 0; j < num_suspects - i - 1; j++) {
            if (suspects[j].suspicion_score < suspects[j + 1].suspicion_score) {
                SuspiciousAgent temp = suspects[j];
                suspects[j] = suspects[j + 1];
                suspects[j + 1] = temp;
            }
        }
    }
    
    // Interrogate suspects (starting with most suspicious)
    int agents_found = 0;
    for (int i = 0; i < num_suspects && i < 3; i++) {  // Limit interrogations to top 3 suspects
        int member_id = suspects[i].member_id;
        int rank = gang->members[member_id].rank;
        
        // Probability of uncovering agent depends on suspicion score and rank
        int discovery_chance = 20 + (gang->members[member_id].rank * 10) + (suspects[i].suspicion_score / 5);
        
        // Cap at 90%
        if (discovery_chance > 90) discovery_chance = 90;
        
        if (gang->members[member_id].is_secret_agent && random_event(discovery_chance)) {
            log_message("Gang %d interrogated and uncovered secret agent %d (rank %d, suspicion: %d)", 
                      gang->id, member_id, rank, suspects[i].suspicion_score);
            
            // Execute the agent
            gang->executed_agents++;
            
            // Replace the agent with a new member
            gang->members[member_id].rank = 0;  // Lowest rank
            gang->members[member_id].preparation_level = 0;
            gang->members[member_id].knowledge_rate = 0;
            gang->members[member_id].is_secret_agent = random_event(config.agent_infiltration_success_rate);
            
            agents_found++;
        } else if (!gang->members[member_id].is_secret_agent) {
            log_message("Gang %d interrogated innocent member %d (rank %d, suspicion: %d)", 
                      gang->id, member_id, rank, suspects[i].suspicion_score);
            
            // Penalty for wrongly accusing a member - they might become less loyal
            if (random_event(40)) {
                gang->members[member_id].preparation_level = (gang->members[member_id].preparation_level * 3) / 4;
                log_message("Member %d lost motivation due to false accusation", member_id);
            }
        }
    }
    
    // If no agents found but we know they exist, increase paranoia for next investigation
    int actual_agents = 0;
    for (int i = 0; i < gang->num_members; i++) {
        if (gang->members[i].is_secret_agent) {
            actual_agents++;
        }
    }
    
    if (agents_found == 0 && actual_agents > 0) {
        log_message("Gang %d failed to find any agents, paranoia increasing", gang->id);
    }
    
    // Free allocated memory
    free(suspects);
    
    pthread_mutex_unlock(&gang->gang_mutex);
}

// Clean up gang resources
void cleanup_gang(Gang* gang) {
    // Set gang as inactive
    gang->is_active = false;
    
    // Signal any waiting threads
    pthread_cond_broadcast(&gang->gang_cond);
    
    // Wait for all threads to finish
    for (int i = 0; i < gang->num_members; i++) {
        pthread_join(gang->members[i].thread, NULL);
    }
    
    // Destroy mutex and condition variable
    pthread_mutex_destroy(&gang->gang_mutex);
    pthread_cond_destroy(&gang->gang_cond);
    
    // Free allocated memory
    free(gang->members);
    
    log_message("Gang %d resources cleaned up", gang->id);
}
