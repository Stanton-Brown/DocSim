# DocSim README

Overview
This project simulates a doctor's office visit, modeling patient flow, resource management, and thread synchronization using semaphores. Patients arrive to the office, register with a receptionist, wait in a waiting room, and are escorted by a nurse to a doctor's office for consultation. Doctors see patients, provide advice, and then the patients leave.


## Usage
Compile the program and run it from the terminal using the following command:
        
        ./docSim  <number_of_doctors> <number_of_patients>
        <number_of_patients>: The number of patients to simulate (integer).
        <number_of_doctors>: The number of doctors available to see patients (integer). 
    
## Implementation
The project is imlempented in C++ utilizing POSIX pthreads and named semaphores for coordinating the threads. The program consists of a single receptionist thread, n doctor and nurse threads where n is command line input, and x patient threads where x is also command line input. The semaphores utilized to coordinate these threads is described in the provided design document. I also utilized queues for the receptionist and each doctors office, and an array that could be indexed with the patients Id to determine which doctors office they had been registered to.

























