#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "../include/config.h"
#include "../include/visualization.h"
#include "../include/gang.h"
#include "../include/police.h"
#include "../include/utils.h"
#include "../include/ipc.h"

// Thread-safe draw_gangs implementation
void draw_gangs(VisualizationContext* ctx) {
    // Thread-safe access to gang information
    int num_gangs;
    GangVisState* gang_states_copy = NULL;
    bool has_states = false;
    float animation_time;
    int window_width, window_height;
    
    pthread_mutex_lock(&ctx->mutex);
    num_gangs = ctx->num_gangs;
    animation_time = ctx->animation_time;
    window_width = ctx->window_width;
    window_height = ctx->window_height;
    
    // Make sure we have gangs to draw
    if (num_gangs <= 0 || ctx->gang_states == NULL) {
        pthread_mutex_unlock(&ctx->mutex);
        
        // Draw a message indicating no gangs
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(window_width / 2 - 50, window_height / 2);
        const char* message = "No gangs to display";
        for (int i = 0; message[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
        }
        return;
    }

    // Make a copy of gang states to avoid holding the mutex during drawing
    gang_states_copy = (GangVisState*)malloc(num_gangs * sizeof(GangVisState));
    if (gang_states_copy) {
        memcpy(gang_states_copy, ctx->gang_states, num_gangs * sizeof(GangVisState));
        has_states = true;
    }
    pthread_mutex_unlock(&ctx->mutex);
    
    if (!has_states) {
        // Memory allocation failed, draw error message
        glColor3f(1.0f, 0.0f, 0.0f);
        glRasterPos2f(window_width / 2 - 100, window_height / 2);
        const char* message = "Error: Failed to allocate memory for visualization";
        for (int i = 0; message[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
        }
        return;
    }
    
    // Draw background grid
    glColor4f(0.2f, 0.2f, 0.3f, 0.3f); // Semi-transparent grid
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < window_width; i += 50) {
        glVertex2f(i, 0);
        glVertex2f(i, window_height);
    }
    for (int i = 0; i < window_height; i += 50) {
        glVertex2f(0, i);
        glVertex2f(window_width, i);
    }
    glEnd();
    
    // Save current matrix
    glPushMatrix();
    
    // Draw all gangs
    for (int i = 0; i < num_gangs; i++) {
        GangVisState* gang_state = &gang_states_copy[i];
        
        // Calculate position based on gang ID
        // Distribute gangs across the screen
        float x_pos = 100.0f + (i % 5) * (window_width - 200) / 5;
        float y_pos = 100.0f + (i / 5) * 150.0f;
        
        // Add subtle motion based on animation time
        x_pos += 10.0f * sin(animation_time * 0.5f + i * 2.0f);
        y_pos += 5.0f * cos(animation_time * 0.7f + i * 1.5f);
        
        // Draw gang icon
        if (gang_state->is_in_prison) {
            // Gang in prison - draw gray
            glColor3f(0.5f, 0.5f, 0.5f);
            
            // Draw prison bars
            glBegin(GL_LINES);
            for (int bar = 0; bar < 5; bar++) {
                float bar_pos = -30.0f + bar * 15.0f;
                glVertex2f(x_pos + bar_pos, y_pos + 40.0f);
                glVertex2f(x_pos + bar_pos, y_pos - 40.0f);
            }
            glEnd();
            
            // Show prison time remaining
            char time_text[32];
            sprintf(time_text, "%d", gang_state->prison_time_remaining);
            glColor3f(1.0f, 1.0f, 1.0f);
            glRasterPos2f(x_pos - 10.0f, y_pos - 60.0f);
            for (char* c = time_text; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
            }
        } else {
            // Normal gang - choose color based on activity
            if (gang_state->preparation_level < 30) {
                // Low preparation - green (planning phase)
                glColor3f(0.2f, 0.8f, 0.2f);
            } else if (gang_state->preparation_level < 70) {
                // Medium preparation - yellow (getting ready)
                glColor3f(0.9f, 0.9f, 0.2f);
            } else {
                // High preparation - red (ready to execute)
                glColor3f(0.9f, 0.2f, 0.2f);
            }
        }
        
        // Draw gang body (circle)
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x_pos, y_pos); // center
        float radius = 30.0f; // Circle radius in pixels
        for (int j = 0; j <= 20; j++) {
            float angle = 2.0f * 3.14159f * j / 20;
            float x = x_pos + radius * cos(angle);
            float y = y_pos + radius * sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
        
        // Draw a contrasting outline to make gang visible
        glColor3f(1.0f, 1.0f, 1.0f); // White outline
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 20; j++) {
            float angle = 2.0f * 3.14159f * j / 20;
            float x = x_pos + radius * cos(angle);
            float y = y_pos + radius * sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
        
        // Draw gang ID
        char id_text[32];
        sprintf(id_text, "Gang %d", gang_state->id);
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x_pos - 25.0f, y_pos - 5.0f);
        for (char* c = id_text; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Draw members as small dots around the gang
        float member_radius = 6.0f;
        float gang_radius = 50.0f;
        
        // Make sure we have at least one member to show
        int members_to_show = (gang_state->num_members > 0) ? gang_state->num_members : 5;
        
        for (int j = 0; j < members_to_show; j++) {
            float angle = 2.0f * 3.14159f * j / members_to_show;
            float member_x = x_pos + gang_radius * cos(angle);
            float member_y = y_pos + gang_radius * sin(angle);
            
            // Is this member an agent?
            bool is_agent = j < gang_state->num_agents;
            
            if (is_agent) {
                // Secret agents pulse (alternate between red and blue)
                float pulse = 0.5f + 0.5f * sin(animation_time * 5.0f + j);
                glColor3f(pulse, 0.0f, 1.0f - pulse);
            } else {
                // Regular members
                glColor3f(1.0f, 1.0f, 1.0f);
            }
            
            // Draw member
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(member_x, member_y); // center
            for (int k = 0; k <= 10; k++) {
                float member_angle = 2.0f * 3.14159f * k / 10;
                float x = member_x + member_radius * cos(member_angle);
                float y = member_y + member_radius * sin(member_angle);
                glVertex2f(x, y);
            }
            glEnd();
        }
        
        // Draw target crime type
        char crime_text[32];
        sprintf(crime_text, "%s", crime_type_to_string(gang_state->current_target));
        glColor3f(0.8f, 0.8f, 0.8f);
        glRasterPos2f(x_pos - 40.0f, y_pos + 50.0f);
        for (char* c = crime_text; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
        }
        
        // Draw preparation level bar
        if (!gang_state->is_in_prison) {
            float prep_width = 60.0f;
            float prep_height = 10.0f;
            float prep_x = x_pos - prep_width / 2;
            float prep_y = y_pos - 50.0f;
            
            // Background bar
            glColor3f(0.3f, 0.3f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(prep_x, prep_y);
            glVertex2f(prep_x + prep_width, prep_y);
            glVertex2f(prep_x + prep_width, prep_y + prep_height);
            glVertex2f(prep_x, prep_y + prep_height);
            glEnd();
            
            // Filled progress bar
            float fill_width = (prep_width * gang_state->preparation_level) / 100.0f;
            
            // Choose color based on preparation level
            if (gang_state->preparation_level < 30) {
                glColor3f(0.2f, 0.7f, 0.2f); // Green for low prep
            } else if (gang_state->preparation_level < 70) {
                glColor3f(0.9f, 0.7f, 0.2f); // Orange for medium prep
            } else {
                glColor3f(0.9f, 0.2f, 0.2f); // Red for high prep
            }
            
            glBegin(GL_QUADS);
            glVertex2f(prep_x, prep_y);
            glVertex2f(prep_x + fill_width, prep_y);
            glVertex2f(prep_x + fill_width, prep_y + prep_height);
            glVertex2f(prep_x, prep_y + prep_height);
            glEnd();
            
            // Show percentage text
            char prep_text[32];
            sprintf(prep_text, "%d%%", gang_state->preparation_level);
            glColor3f(1.0f, 1.0f, 1.0f);
            glRasterPos2f(prep_x + prep_width/2 - 10.0f, prep_y + prep_height/2); 
            for (char* c = prep_text; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
            }
        }
    }
    
    glPopMatrix();
    
    // Free the temporary copy we made
    free(gang_states_copy);
}
