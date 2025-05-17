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
#include "../include/ipc.h"

// Global visualization context is declared as extern in the header
// No need to redefine it here

// Colors for different entities
float gang_colors[7][3] = {
    {1.0f, 0.0f, 0.0f},  // Red
    {0.0f, 0.0f, 1.0f},  // Blue
    {0.0f, 1.0f, 0.0f},  // Green
    {1.0f, 1.0f, 0.0f},  // Yellow
    {1.0f, 0.0f, 1.0f},  // Magenta
    {0.0f, 1.0f, 1.0f},  // Cyan
    {1.0f, 0.5f, 0.0f}   // Orange
};

// Initialize OpenGL visualization
void initialize_visualization(int* argc, char** argv, VisualizationContext* ctx) {
    // Set environment variable to force software rendering
    // This can help with X server issues in some environments
    putenv("LIBGL_ALWAYS_SOFTWARE=1");
    
    // Print some debug info about the display
    char* display = getenv("DISPLAY");
    printf("DISPLAY environment variable: %s\n", display ? display : "not set");
    
    // Try to initialize GLUT
    glutInit(argc, argv);
    
    // Try to initialize display mode
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    
    // Set window size and position
    ctx->window_width = 800;
    ctx->window_height = 600;
    glutInitWindowSize(ctx->window_width, ctx->window_height);
    glutInitWindowPosition(100, 100);
    
    // Create window with error checking
    int window_id = glutCreateWindow("Organized Crime Fighting Simulation");
    if (window_id <= 0) {
        fprintf(stderr, "Error: Failed to create GLUT window\n");
        // Don't terminate, just note the failure
        ctx->simulation_running = false;
        return;
    }
    
    // Set callback functions
    glutDisplayFunc(display_function);
    glutReshapeFunc(reshape_function);
    glutIdleFunc(idle_function);  // Add idle function for continuous rendering
    
    // Set up timer for simulation updates
    glutTimerFunc(ctx->refresh_rate, timer_function, 0);
    
    // Set up clear color (black)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Save context globally
    viz_context = *ctx;
    viz_context.simulation_running = true;  // Ensure this is set
    
    printf("OpenGL visualization initialized successfully\n");
    // We don't call glutMainLoop() here anymore
    // It will be called in the visualization thread
}

// Display callback function
void display_function() {
    // Thread-safe access to visualization context
    bool simulation_running;
    pthread_mutex_lock(&viz_context.mutex);
    simulation_running = viz_context.simulation_running;
    pthread_mutex_unlock(&viz_context.mutex);
    
    // Check if the visualization context is properly initialized
    if (!simulation_running) {
        // Something's wrong, just clear the screen to black and return
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glutSwapBuffers();
        return;
    }

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Reset modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Only print every 10 frames to avoid console spam
    static int frame_count = 0;
    if (frame_count++ % 10 == 0) {
        pthread_mutex_lock(&viz_context.mutex);
        printf("Rendering frame %d with %d gangs...\n", frame_count, viz_context.num_gangs);
        pthread_mutex_unlock(&viz_context.mutex);
    }
    
    // Draw simulation elements
    draw_gangs(&viz_context);
    draw_police(&viz_context);
    draw_stats(&viz_context);
    draw_debug_info(&viz_context);
    
    // Draw simulation status bar at the top
    draw_status_bar(&viz_context);
    
    // Disable blending
    glDisable(GL_BLEND);
    
    // Swap buffers
    glutSwapBuffers();
}

// Reshape callback function
void reshape_function(int width, int height) {
    // Update window size
    viz_context.window_width = width;
    viz_context.window_height = height;
    
    // Set viewport to cover the new window
    glViewport(0, 0, width, height);
    
    // Set up the projection matrix for pixel coordinates
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Use window coordinates with origin at bottom-left
    // OpenGL's default origin is bottom-left, so y-axis goes up
    gluOrtho2D(0.0, (GLdouble)width, 0.0, (GLdouble)height);
    
    // Switch back to modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    printf("Resized window to %d x %d pixels\n", width, height);
}

// Timer callback function for simulation updates
void timer_function(int value) {
    // Thread-safe access to visualization data
    bool simulation_running;
    pthread_mutex_lock(&viz_context.mutex);
    simulation_running = viz_context.simulation_running;
    
    // Only update if simulation is still running
    if (simulation_running) {
        // Update animation time is now done in the visualization thread
        // to avoid thread safety issues
        
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

// Function to draw police on the screen
void draw_police(VisualizationContext* ctx) {
    // Police headquarters at the top of the screen
    float hq_x = ctx->window_width / 2;
    float hq_y = ctx->window_height - 50;
    float hq_width = 100.0f;
    float hq_height = 40.0f;
    
    // Set police color (blue)
    glColor3f(0.0f, 0.0f, 0.8f);
    
    // Draw police headquarters
    glBegin(GL_QUADS);
    glVertex2f(hq_x - hq_width/2, hq_y - hq_height/2);
    glVertex2f(hq_x + hq_width/2, hq_y - hq_height/2);
    glVertex2f(hq_x + hq_width/2, hq_y + hq_height/2);
    glVertex2f(hq_x - hq_width/2, hq_y + hq_height/2);
    glEnd();
    
    // Draw "POLICE" text
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(hq_x - 25, hq_y - 5);
    const char* police_text = "POLICE";
    for (int i = 0; i < strlen(police_text); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, police_text[i]);
    }
}

// Function to draw statistics on the screen
void draw_stats(VisualizationContext* ctx) {
    // Get statistics from shared memory if available
    int successful_missions = 0;
    int thwarted_missions = 0;
    int executed_agents = 0;
    
    if (ctx->shared_state != NULL) {
        successful_missions = ctx->shared_state->total_successful_missions;
        thwarted_missions = ctx->shared_state->total_thwarted_missions;
        executed_agents = ctx->shared_state->total_executed_agents;
    }
    
    // Set text color
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Draw stats at the bottom of the screen
    float text_x = 10;
    float text_y = 20;
    
    // Draw successful missions
    glRasterPos2f(text_x, text_y);
    char buffer[100];
    sprintf(buffer, "Successful Missions: %d/%d", successful_missions, ctx->config.max_successful_plans);
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
    
    // Draw thwarted missions
    glRasterPos2f(text_x, text_y + 15);
    sprintf(buffer, "Thwarted Missions: %d/%d", thwarted_missions, ctx->config.max_thwarted_plans);
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
    
    // Draw executed agents
    glRasterPos2f(text_x, text_y + 30);
    sprintf(buffer, "Executed Agents: %d/%d", executed_agents, ctx->config.max_executed_agents);
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
}

// Function to draw a status bar at the top of the screen
void draw_status_bar(VisualizationContext* ctx) {
    // Draw a background for the status bar
    glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(0, ctx->window_height - 30);
    glVertex2f(ctx->window_width, ctx->window_height - 30);
    glVertex2f(ctx->window_width, ctx->window_height);
    glVertex2f(0, ctx->window_height);
    glEnd();
    
    // Draw simulation status
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(10, ctx->window_height - 20);
    
    char buffer[100];
    
    // Show current time
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    sprintf(buffer, "Simulation Time: %02d:%02d:%02d | Status: %s", 
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
            ctx->simulation_running ? "Running" : "Stopped");
    
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
    
    // Show termination condition if available
    if (ctx->shared_state != NULL) {
        if (ctx->shared_state->total_successful_missions >= ctx->config.max_successful_plans) {
            glColor3f(1.0f, 0.5f, 0.0f); // Orange for gangs winning
            glRasterPos2f(ctx->window_width - 200, ctx->window_height - 20);
            sprintf(buffer, "Gangs Win! (%d missions)", ctx->shared_state->total_successful_missions);
        } else if (ctx->shared_state->total_thwarted_missions >= ctx->config.max_thwarted_plans) {
            glColor3f(0.0f, 0.7f, 1.0f); // Blue for police winning
            glRasterPos2f(ctx->window_width - 200, ctx->window_height - 20);
            sprintf(buffer, "Police Win! (%d thwarts)", ctx->shared_state->total_thwarted_missions);
        } else if (ctx->shared_state->total_executed_agents >= ctx->config.max_executed_agents) {
            glColor3f(1.0f, 0.0f, 0.0f); // Red for agents executed
            glRasterPos2f(ctx->window_width - 200, ctx->window_height - 20);
            sprintf(buffer, "Agents Lost! (%d executed)", ctx->shared_state->total_executed_agents);
        } else {
            // Still running
            buffer[0] = '\0';
        }
        
        for (int i = 0; i < strlen(buffer); i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
        }
    }
}

// Function to add visual debugging indicators
void draw_debug_info(VisualizationContext* ctx) {
    // Set text color
    glColor3f(1.0f, 1.0f, 0.0f); // Yellow text for visibility
    
    // Draw at top-left corner
    float text_x = 10;
    float text_y = ctx->window_height - 50;
    
    // Display number of gangs and animation time
    char buffer[100];
    sprintf(buffer, "Debug: %d gangs, %.1f anim time", ctx->num_gangs, ctx->animation_time);
    glRasterPos2f(text_x, text_y);
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
    
    // Display address of gang states
    sprintf(buffer, "Gang states: %p", (void*)ctx->gang_states);
    glRasterPos2f(text_x, text_y - 15);
    for (int i = 0; i < strlen(buffer); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, buffer[i]);
    }
    
    // Draw coordinate system reference
    glColor3f(1.0f, 0.0f, 0.0f); // Red for X axis
    glBegin(GL_LINES);
    glVertex2f(50, 50);
    glVertex2f(150, 50);
    glEnd();
    glColor3f(0.0f, 1.0f, 0.0f); // Green for Y axis
    glBegin(GL_LINES);
    glVertex2f(50, 50);
    glVertex2f(50, 150);
    glEnd();
    
    // Draw coordinate labels
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(150, 55);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, 'X');
    glRasterPos2f(55, 150);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, 'Y');
}

// Cleanup visualization resources
void cleanup_visualization() {
    // Nothing to clean up for OpenGL in this basic implementation
}
