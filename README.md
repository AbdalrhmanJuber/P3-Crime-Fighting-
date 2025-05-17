# Organized Crime-Fighting Simulation

This is a multi-process and multi-threading application that simulates the behavior of infiltrated secret agents in fighting organized crime gangs.

## Project Structure

- `src/`: Source code files
- `include/`: Header files
- `config/`: Configuration files
- `build/`: Build output directory

## Requirements

- GCC compiler
- POSIX thread library (pthread)
- OpenGL libraries (GL, GLU, GLUT)

## Installation

### Install dependencies

On Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install build-essential libgl1-mesa-dev freeglut3-dev
```

## Building the Project

To build the project, simply run:
```bash
make
```

This will create the executable `build/crime_sim`.

## Running the Simulation

To run the simulation with the default configuration:
```bash
make run
```

Alternatively, you can run the executable directly:
```bash
./build/crime_sim config/simulation_config.txt
```

## Configuration

The simulation parameters can be modified in the `config/simulation_config.txt` file. You can adjust:

- Number of gangs and gang members
- Gang ranking structure
- Crime planning parameters
- Secret agent infiltration rates
- Mission success/failure probabilities
- Prison time
- Termination conditions

## Debugging

To build with debugging symbols:
```bash
make debug
```

To use GDB for debugging:
```bash
gdb ./build/crime_sim
```

## Visualization

The simulation uses OpenGL for visualization. The display shows:
- Gang structures with members and ranks
- Secret agents (infiltrated within gangs)
- Police headquarters
- Current statistics (successful missions, thwarted plans, executed agents)

## How It Works

1. The main program creates multiple gang processes and a police process
2. Each gang has multiple members (threads)
3. Some gang members are secretly police agents
4. Gangs plan and execute criminal activities
5. Secret agents collect information and report to police
6. Police analyze reports and take action to thwart gang plans
7. Gangs conduct internal investigations to uncover secret agents

The simulation ends when one of the termination conditions is met.

## Project Components

1. **Gang Module**: Manages gang creation, member hierarchy, and criminal activities
2. **Police Module**: Handles intelligence gathering and actions against gangs
3. **Visualization Module**: Provides graphical representation of the simulation
4. **Configuration Module**: Loads user-defined parameters
5. **IPC Module**: Handles inter-process communication
6. **Utility Module**: Provides common functions used throughout the application
