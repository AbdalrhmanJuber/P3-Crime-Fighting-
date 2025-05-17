#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <GL/glut.h>
#include <stdbool.h>
#include <pthread.h>  // Add this for pthread_mutex_t
#include "config.h"
#include "ipc.h"

// Forward declarations to avoid circular dependencies
struct Gang;
struct Police;

// Gang visualization state
typedef struct {
    int id;
    bool is_in_prison;
    int prison_time_remaining;
    int preparation_level;
    CrimeType current_target;
    int num_members;
    int num_agents;
    bool is_active;
} GangVisState;

// Visualization context structure
typedef struct {
    struct Gang* gangs;           // Gangs in the simulation
    int num_gangs;               // Number of gangs
    struct Police* police;        // Police in the simulation
    SimulationConfig config;     // Simulation configuration
    bool simulation_running;     // Flag to indicate if simulation is running
    int refresh_rate;            // Refresh rate in milliseconds
    int window_width;            // Window width
    int window_height;           // Window height
    float animation_time;        // Time for animation purposes
    GangVisState* gang_states;   // Gang visualization states
    SharedState* shared_state;   // Shared state for IPC
    pthread_mutex_t mutex;       // Mutex for thread-safe access to visualization data
    bool viz_thread_running;     // Flag to indicate if visualization thread is running
    int viz_thread_health;       // Counter for thread health checks
} VisualizationContext;

// Global visualization context
extern VisualizationContext viz_context;

// Function prototypes
void initialize_visualization(int* argc, char** argv, VisualizationContext* ctx);
void display_function();
void reshape_function(int width, int height);
void timer_function(int value);
void idle_function();
void draw_gangs(VisualizationContext* ctx);
void draw_police(VisualizationContext* ctx);
void draw_stats(VisualizationContext* ctx);
void draw_status_bar(VisualizationContext* ctx);
void draw_debug_info(VisualizationContext* ctx);
void cleanup_visualization();

#endif /* VISUALIZATION_H */
