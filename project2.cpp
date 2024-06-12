/*
Program: Hospital Simulator
File:    project2.cpp
Author:  Stanton Brown
Date:    4/2/24
Description:
    This C++ program simulates a hospital setting with patients, doctors, nurses, and a receptionist. 
    It utilizes threads and semaphores to manage concurrent processes and synchronize access to shared resources.

Threads:
    Receptionist: Registers patients and assigns them to doctors' offices.
    Patient: Enters the waiting room, registers, waits for a nurse, sees a doctor, and leaves.
    Doctor: Sees patients in their office, listens to symptoms, and provides diagnosis.
    Nurse: Escorts patients from the waiting room to the assigned doctor's office.

Semaphores:
    Control access to critical sections like the waiting room, doctor's offices, and shared variables.
    Ensure proper synchronization between threads and prevent race conditions.

Components:
    Patient: Represents a patient entering the hospital, waiting, seeing a doctor, and leaving.
    Doctor: Represents a doctor seeing patients in their office.
    Nurse: Represents a nurse who escorts patients to doctor's offices.
    Receptionist: Registers patients and assigns them to doctors.
    Queues: Manage patients waiting for the receptionist and waiting for doctors.
    Semaphores: Synchronize access to shared resources and manage thread interactions.

Functionality:
    Patients enter the waiting room (limited capacity).
    The receptionist registers patients and assigns them to a doctor's office.
    Nurses escort patients from the waiting room to the assigned doctor's office.
    Doctors see patients, listen to symptoms, and provide diagnoses.
    Patients leave the doctor's office and exit the hospital.
    The program simulates a realistic hospital workflow with concurrent patient processing.

Usage:
    Compile the program and run it from the terminal using the following command:
        ./project2  <number_of_doctors> <number_of_patients>
        <number_of_patients>: The number of patients to simulate (integer).
        <number_of_doctors>: The number of doctors available to see patients (integer).
*/
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <stdexcept>
#include <queue>
#include <fcntl.h>

#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <random>

using namespace std;

int docNum;     // Specifies the number of doctor offices
int patientNum; // Specifies the number of patients

queue<int> recepQueue;      // Queue of patients waiting to register with receptionist
queue<int> officeQueues[3]; // Queues of patients waiting for their respective office

int assignedOffice[15]; // Used to store what office the patient was assigned to during registration

sem_t *capacitySem; // Used to ensure the waiting room does not exceed capacity
sem_t *printSem;    // Used to protect threads printing

sem_t *pwrSem; // Indicates if patients re waiting on receptionist

sem_t *recepSem;    // Indicates if receptionist is available
sem_t *registerSem; // Indicates if the patient has been registered with an office
sem_t *wrSem;       // Indicates when the patient enters the waiting room

sem_t *enterSem; // Indicates when the patient enters their assigned doctors office

sem_t *docSems[3];       // Control access to the doctors
sem_t *nurseSems[3];     // Control access to the nurses
sem_t *officeSems[3];    // Control access to the office
sem_t *patientSems[3];   // Indicate how many patients are waiting for office
sem_t *assesmentSems[3]; // Indicate when the doctors assesment is complete

/*
 * Function: patient
 * --------------------
 * Simulates the actions a patient would take
 * Parameters:
 * - *arg: A pointer to an integer containing the unique identifier of the patient thread.
 * Returns Value:
 * - The function returns the argument pointer (`arg`)
 */
void *patient(void *arg)
{
    // Wait until there is room in the waiting room
    sem_wait(capacitySem);
    int patientId = *(int *)arg;

    // Enter waiting room
    sem_wait(recepSem);
    recepQueue.push(patientId);
    sem_wait(printSem);
    cout << "Patient " << patientId << " enters waiting room, waits for receptionist" << endl;
    sem_post(printSem);

    // Signal to receptionist that a patient is waiting
    sem_post(pwrSem);

    // Wait for the receptionist to complete registration
    sem_wait(registerSem);
    sem_wait(printSem);
    cout << "Patient " << patientId << " leaves receptionist and sits in waiting room" << endl;
    sem_post(printSem);

    // Signal that patient has entered waiting room
    sem_post(wrSem);

    // Signal the next patient that the receptionist is available
    sem_post(recepSem);

    // wait for nurse to escort patient to office
    sem_wait(nurseSems[assignedOffice[patientId]]);
    sem_wait(printSem);
    cout << "Patient " << patientId << " enters doctor " << assignedOffice[patientId] << "'s office" << endl;
    sem_post(printSem);

    // Enter office
    sem_post(enterSem);

    // Wait for doctor to perform assessment ? this is the problem!!! we are having both the nurse and patient wait on doc
    sem_wait(assesmentSems[assignedOffice[patientId]]);
    sem_wait(printSem);
    cout << "Patient " << patientId << " receives advice from doctor " << assignedOffice[patientId] << endl;
    sem_post(printSem);

    // Patient leaves office
    sem_wait(printSem);
    cout << "Patient " << patientId << " leaves" << endl;
    sem_post(printSem);


    // Make office available again
    sem_post(officeSems[assignedOffice[patientId]]);

    // Increase capacity
    sem_post(capacitySem);

    return arg;
}

/*
 * Function: receptionist
 * --------------------
 * Simulates the actions a receptionist would take
 * Parameters:
 * - *arg: A pointer to an integer containing the unique identifier of the receptionist thread.
 * Returns Value:
 * - The function returns the argument pointer (`arg`)
 */
void *receptionist(void *arg)
{
    int recepNum = patientNum; // Used for exit condition

    // Seed the random number generator for office assignment
    int ranOffice;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, docNum - 1);

    while (true)
    {
        // wait for next patient
        sem_wait(pwrSem);

        // Get a random office number
        ranOffice = dis(gen);

        // Register patient
        int patientId = recepQueue.front();
        sem_wait(printSem);
        cout << "Receptionist registers patient " << patientId << endl;
        sem_post(printSem);
        recepQueue.pop();

        // Store the random office for patient thread to access
        assignedOffice[patientId] = ranOffice;

        // Push the patient to the random offices queue
        officeQueues[ranOffice].push(patientId);

        // Signal that patient is registered and can now go to waiting room
        sem_post(registerSem);

        // Wait for patient to arrive in waiting room
        sem_wait(wrSem);

        // Signal the nurse a patient is waiting for their assigned office
        sem_post(patientSems[ranOffice]);

        // Exit once all patients are registered
        if (--recepNum == 0)
        {
            break;
        }
    }

    // cout << "Receptionist thread exiting" << endl;

    return arg;
}

/*
 * Function: nurse
 * --------------------
 * Simulates the actions a nurse would take
 * Parameters:
 * - *arg: A pointer to an integer containing the unique identifier of the nurse thread.
 * Returns Value:
 * - The function returns the argument pointer (`arg`)
 */
void *nurse(void *arg)
{
    int nurseNum = patientNum; // Used for exit condition
    int nurseId = *(int *)arg;

    while (true)
    {
        // Wait for receptionist to notify nurse
        sem_wait(patientSems[nurseId]);

        // Take patient to docs office
        sem_wait(printSem);
        cout << "Nurse " << nurseId << " takes patient " << officeQueues[nurseId].front() << " to doctor's office" << endl;
        sem_post(printSem);

        // Signal patient has arrived to office
        sem_post(nurseSems[nurseId]);

        // Wait until patient has entered office
        sem_wait(enterSem);

        // Notify doctor patient is waiting in office
        sem_post(officeSems[nurseId]);
        // sem_post(patientSems[nurseId]);

        // Wait for doctor to be complete
        sem_wait(docSems[nurseId]);

        // Exit once all patients have been taken to their offices
        if (--nurseNum == 0)
        {
            break;
        }
    }
    // cout << "Nurse thread " << nurseId << " exiting" << endl;
    return arg;
}

/*
 * Function: doctor
 * --------------------
 * Simulates the actions a doctor would take
 * Parameters:
 * - *arg: A pointer to an integer containing the unique identifier of the doctor thread.
 * Returns Value:
 * - The function returns the argument pointer (`arg`)
 */
void *doctor(void *arg)
{
    int docNum = patientNum; // Used for exit condition
    int patient;
    int doctorId = *(int *)arg;

    // Spawn nurse for this doctor
    pthread_t nurseThread;
    pthread_create(&nurseThread, NULL, nurse, &doctorId);

    while (true)
    {
        // Wait for nurse to notify patient is waiting in office
        sem_wait(officeSems[doctorId]);

        // Get patient id from queue
        patient = officeQueues[doctorId].front();

        // Listen to symtoms from patient
        sem_wait(printSem);
        cout << "Doctor " << doctorId << " listens to symptoms from patient " << patient << endl;
        sem_post(printSem);

        // Signal the assesment is done and relay diagnosis to patient
        sem_post(assesmentSems[doctorId]);

        // Remove patient id from queue
        officeQueues[doctorId].pop();

        // Wait for patient to leave office
        sem_wait(officeSems[doctorId]);

        // make self available again
        sem_post(docSems[doctorId]);

        // Exit once all patients have left office
        if (--docNum == 0)
        {
            break;
        }
    }

    // cout << "Doctor thread " << doctorId << " exiting" << endl;

    /* return arg back to the join function */
    return arg;
}

/*
 * Function: cleanUp
 * --------------------
 * Ensures that all named semaphores are closed and unlinked.
 */
void cleanUp()
{
    sem_close(recepSem);
    sem_close(pwrSem);
    sem_close(registerSem);
    sem_close(enterSem);
    sem_close(wrSem);
    sem_close(printSem);
    sem_close(capacitySem);
    sem_unlink("/capacitySem");
    sem_unlink("/printSem");
    sem_unlink("/wrSem");
    sem_unlink("/enterSem");
    sem_unlink("/recepSem");
    sem_unlink("/pwrSem");
    sem_unlink("/registerSem");

    for (int i = 0; i < 3; i++)
    {
        string semName = "/docSem" + to_string(i);
        string semNames = "/nurseSem" + to_string(i);
        string semN = "/officeSem" + to_string(i);
        string semP = "/patientSem" + to_string(i);
        string semA = "/assesmentSem" + to_string(i);

        sem_close(assesmentSems[i]);
        sem_close(patientSems[i]);
        sem_close(officeSems[i]);
        sem_close(docSems[i]);
        sem_close(nurseSems[i]);

        sem_unlink(semA.c_str());
        sem_unlink(semP.c_str());
        sem_unlink(semN.c_str());
        sem_unlink(semName.c_str());
        sem_unlink(semNames.c_str());
    }
}

int main(int argc, char *argv[])
{

    cleanUp();

    // Initialize each semaphore in the arrays
    for (int i = 0; i < 3; i++)
    {
        string semNurse = "/nurseSem" + to_string(i);
        nurseSems[i] = sem_open(semNurse.c_str(), O_CREAT | O_EXCL, 0644, 0);
        if (nurseSems[i] == SEM_FAILED)
        {
            cerr << "Failed to create : " << semNurse << " " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        string semAssesment = "/assesmentSem" + to_string(i);
        assesmentSems[i] = sem_open(semAssesment.c_str(), O_CREAT | O_EXCL, 0644, 0);
        if (assesmentSems[i] == SEM_FAILED)
        {
            cerr << "Failed to create : " << semAssesment << " " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        string semDoc = "/docSem" + to_string(i);
        docSems[i] = sem_open(semDoc.c_str(), O_CREAT | O_EXCL, 0644, 1);
        if (docSems[i] == SEM_FAILED)
        {
            cerr << "Failed to create : " << semDoc << " " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        string semOffice = "/officeSem" + to_string(i);
        officeSems[i] = sem_open(semOffice.c_str(), O_CREAT | O_EXCL, 0644, 0);
        if (officeSems[i] == SEM_FAILED)
        {
            cerr << "Failed to create : " << semOffice << " " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        string semPatient = "/patientSem" + to_string(i);
        patientSems[i] = sem_open(semPatient.c_str(), O_CREAT | O_EXCL, 0644, 0);
        if (patientSems[i] == SEM_FAILED)
        {
            cerr << "Failed to create : " << semPatient << " " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
    }

    capacitySem = sem_open("/capacitySem", O_CREAT | O_EXCL, 0644, 15);
    if (capacitySem == SEM_FAILED)
    {
        cerr << "Failed to create capacitySem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    printSem = sem_open("/printSem", O_CREAT | O_EXCL, 0644, 1);
    if (printSem == SEM_FAILED)
    {
        cerr << "Failed to create printSem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    registerSem = sem_open("/registerSem", O_CREAT | O_EXCL, 0644, 0);
    if (registerSem == SEM_FAILED)
    {
        cerr << "Failed to create recepSem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    recepSem = sem_open("/recepSem", O_CREAT | O_EXCL, 0644, 1);
    if (recepSem == SEM_FAILED)
    {
        cerr << "Failed to create recepSem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    pwrSem = sem_open("/pwrSem", O_CREAT | O_EXCL, 0644, 0);
    if (pwrSem == SEM_FAILED)
    {
        cerr << "Failed to create pwrSem: " << strerror(errno) << endl;
        sem_unlink("/recepSem");
        return EXIT_FAILURE;
    }

    enterSem = sem_open("/enterSem", O_CREAT | O_EXCL, 0644, 0);
    if (enterSem == SEM_FAILED)
    {
        cerr << "Failed to create enterSem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    wrSem = sem_open("/wrSem", O_CREAT | O_EXCL, 0644, 0);
    if (wrSem == SEM_FAILED)
    {
        cerr << "Failed to create wrSem: " << strerror(errno) << endl;
        return EXIT_FAILURE;
    }

    // Check for proper usage
    if (argc != 3)
    {
        cerr << "Usage: " << argv[0] << " <> <timer>" << endl;
        _exit(1);
    }

    // Ensure argumetn is an integer
    try
    {
        stringstream container(argv[2]);
        int x;
        container >> patientNum;
        // cout << "Value of patient number: " << patientNum;
    }
    catch (const invalid_argument &e)
    {

        // Failed to convert
        cerr << "ERROR: Second argument must be an integer" << endl;
        cerr << "Exiting..." << endl;
        _exit(1);
    }

    // Ensure argumetn is an integer
    try
    {
        stringstream container(argv[1]);
        int x;
        container >> docNum;
        // cout << "Value of x: " << docNum;
    }
    catch (const invalid_argument &e)
    {

        // Failed to convert
        cerr << "ERROR: First argument must be an integer" << endl;
        cerr << "Exiting..." << endl;
        _exit(1);
    }

    cout << "Run with " << patientNum << " patients, "
         << docNum << " nurses, "
         << docNum << " doctors" << endl
         << endl;

    queue<int> officeQueues[docNum];

    // Create receptionist thread
    pthread_t recThread;
    pthread_create(&recThread, NULL, receptionist, NULL);

    // Create patient threads
    int p;
    pthread_t patientThreads[patientNum];
    int patientIds[patientNum]; 

    for (p = 0; p < patientNum; p++)
    {
        patientIds[p] = p;
        pthread_create(&patientThreads[p],
                       NULL,
                       patient,
                       &patientIds[p]);
    }

    // Create the doctor threads
    int doc;
    pthread_t docThreads[docNum];
    int docIds[docNum];

    for (doc = 0; doc < docNum; doc++)
    {
        docIds[doc] = doc;
        pthread_create(&docThreads[doc],
                       NULL,   /* default thread attributes */
                       doctor, /* start routine             */
                       &docIds[doc]);
    }

    // Join threads
    int status;

    status = pthread_join(recThread, NULL);
    if (status != 0)
    {
        printf("ERROR: Joining receptionist thread\n");
        exit(1);
    }

    for (p = 0; p < patientNum; p++)
    {
        status = pthread_join(patientThreads[p], NULL);
        if (status != 0)
        {
            printf("ERROR: Joining pateint threads\n");
            exit(1);
        }
    }

    cout << "Simulation complete" << endl;
    return(0);

}
