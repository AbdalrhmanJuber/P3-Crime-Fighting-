#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "../include/config.h"
#include "../include/visualization.h"
#include "../include/gang.h"
#include "../include/police.h"
#include "../include/utils.h"

// Global visualization context
VisualizationContext viz_context;

// Gang colors for visual variety
float gang_colors[7][3] = {
    {0.8f, 0.2f, 0.2f}, // Red
    {0.2f, 0.8f, 0.2f}, // Green
    {0.2f, 0.2f, 0.8f}, // Blue
    {0.8f, 0.8f, 0.2f}, // Yellow
    {0.8f, 0.2f, 0.8f}, // Purple
    {0.2f, 0.8f, 0.8f}, // Cyan
    {0.8f, 0.6f, 0.2f}  // Orange
};

// Initialize visualization with OpenGL
void initialize_visualization(int* argc, char** argv, VisualizationContext* ctx) {
    // Initialize mutex for thread-safe operations
    pthread_mutex_init(&ctx->mutex, NULL);
    
    // Set default window size
    ctx->window_width = 1024;
    ctx->window_height = 768;
    
    // Initialize animation time
    ctx->animation_time = 0.0f;
    
    // Initialize GLUT
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(ctx->window_width, ctx->window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Crime Simulation");
    
    // Register callbacks
    glutDisplayFunc(display_function);
    glutReshapeFunc(reshape_function);
    glutTimerFunc(ctx->refresh_rate, timer_function, 0);
    glutIdleFunc(idle_function);
    
    // Initialize OpenGL
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f); // Dark blue background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, ctx->window_width, 0, ctx->window_height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    printf("Visualization initialized with size %d x %d\n", ctx->window_width, ctx->window_height);
    
    // Set refresh rate
    ctx->refresh_rate = 50; // 50ms = 20fps
    ctx->viz_thread_running = true;
    ctx->viz_thread_health = 0;
}

// Display callback function for rendering
void display_function() {
    // Clear the window
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Update thread health counter to detect freezes
    pthread_mutex_lock(&viz_context.mutex);
    viz_context.viz_thread_health++;
    pthread_mutex_unlock(&viz_context.mutex);
    
    // Draw simulation elements
    draw_gangs(&viz_context);
    draw_police(&viz_context);
    draw_stats(&viz_context);
    draw_status_bar(&viz_context);
    
    #ifdef DEBUG
    draw_debug_info(&viz_context);
    #endif
    
    // Swap buffers to display what we just drew
    glutSwapBuffers();
}

// Reshape callback function for window resizing
void reshape_function(int width, int height) {
    // Update stored window dimensions
    pthread_mutex_lock(&viz_context.mutex);
    viz_context.window_width = width;
    viz_context.window_height = height;
    pthread_mutex_unlock(&viz_context.mutex);
    
    // Update viewport
    glViewport(0, 0, width, height);
    
    // Update projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    printf("Resized window to %d x %d pixels\n", width, height);
}

// Timer callback function for simulation updates
void timer_function(int value) {
    // Check if visualization context is valid
    if (!viz_context.viz_thread_running) {
        // Something's very wrong, set up a dummy timer to keep things going
        glutTimerFunc(1000, timer_function, 0);
        return;
    }

    // Thread-safe access to visualization data
    bool simulation_running;
    pthread_mutex_lock(&viz_context.mutex);
    simulation_running = viz_context.simulation_running;
    
    // Only update if simulation is still running
    if (simulation_running) {
        // Update animation time
        viz_context.animation_time += 0.1f;
        
        // Debug animation timer every 10 frames to avoid console spam
        if ((int)(viz_context.animation_time * 10) % 10 == 0) {
            printf("Animation time: %.1f, Gangs: %d\n", 
                  viz_context.animation_time, viz_context.num_gangs);
        }
    }
    pthread_mutex_unlock(&viz_context.mutex);
    
    // Check if simulation is still running
    if (simulation_running) {
        // Force a redraw to update the visualization
        glutPostRedisplay();
        
        // Set up next timer
        glutTimerFunc(viz_context.refresh_rate, timer_function, 0);
    } else {
        // If simulation is no longer running, we could exit, but let's just stop the timer
        printf("Simulation stopped, visualization will no longer update\n");
    }
}

// Idle function to ensure continuous rendering
void idle_function() {
    // Request a redraw
    glutPostRedisplay();
}

// Function to clean up visualization resources
void cleanup_visualization() {
    // Clean up mutex
    pthread_mutex_destroy(&viz_context.mutex);
    
    // Free allocated memory
    if (viz_context.gang_states != NULL) {
        free(viz_context.gang_states);
        viz_context.gang_states = NULL;
    }
    
    printf("Visualization resources cleaned up\n");
}
