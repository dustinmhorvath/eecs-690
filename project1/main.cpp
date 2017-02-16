#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include "barrier.h"

std::mutex coutMutex;
std::mutex trainDecrementMutex;
std::mutex** trainLineMutex;
bool chillForASec = true;
Barrier b;
// Total trains, which I want accessible to threads.
int numTrains;
// Total station count
int maxStations;
int trainCounter = 0;


void choochoo(int trainNumber, int numStations, int* stationList){

  coutMutex.lock();
  std::cout << "I am train " << trainNumber << " and my stationlist is ";
  for(int i = 0; i < numStations; i++){
    std::cout << stationList[i] << " ";
  }
  std::cout << "\n";
  coutMutex.unlock();
 
  b.barrier(numTrains);
  b.barrier(numTrains);

  while(chillForASec);

  int currentStationIndex = 0;
  int timeStep = 0;
  while(currentStationIndex < numStations-1){
    // Verbose for sanity's sake
    int currentStation = stationList[currentStationIndex];
    int nextStation = stationList[currentStationIndex+1];
    // Lock on track required. I'm using the notation that each track is
    //  [lowernumberstation][highernumberstation], regardless of travelling
    //  direction.
    std::unique_lock<std::mutex> trackLock(trainLineMutex[std::min(currentStation, nextStation)][std::max(currentStation, nextStation)], std::defer_lock);

    if(trackLock.try_lock()){

      coutMutex.lock();      
      //std::cout << "At time step: " << timeStep << " train " << trainNumber << " is going from station " << currentStation << " to station " << nextStation << "\n";
      std::cout << "At time step: " << timeStep << " train " << trainNumber << " is going from station " << currentStation << " to station " << nextStation << " locking [" << std::min(currentStation, nextStation) << "][" << std::max(currentStation, nextStation) << "]\n";
      coutMutex.unlock();

      currentStationIndex++;
    }
    else{
      coutMutex.lock();
      std::cout << "At time step: " << timeStep << " train " << trainNumber << " must stay at station " << currentStation << "\n";
      coutMutex.unlock();
    }
    timeStep++;

    // TODO sooomething before this barrier
    

    int oldNumTrains = numTrains;
    // Keep in sync
    b.barrier(numTrains);
    
    // Unlock after done
    if(trackLock.owns_lock()){
      trackLock.unlock();
    }
    if(trainNumber == 0){
      std::cout << "\n";
    }

    std::unique_lock<std::mutex> decrementUniqueLock(trainDecrementMutex);
    if(currentStationIndex == numStations-1){
      numTrains--;
    }
    trainCounter++;
    decrementUniqueLock.unlock();

    b.barrier(oldNumTrains);

      while(trainCounter < oldNumTrains);

    b.barrier(oldNumTrains);

      trainCounter = 0;

    b.barrier(oldNumTrains);
    
  }

 
}

int main(int argc, char* argv[])
{

  // Read data file from std
  if(argc < 2){
    std::cout << "Please provide a valid input data file\n";
    exit(0);
  }

  std::ifstream inFile(argv[1]);
  std::string line;

  // Array holding length of route for each train
  int queueLength[numTrains];
  std::getline(inFile, line);
  std::istringstream issQuick(line);
  issQuick >> numTrains;
  issQuick >> maxStations;

  // Instantiate array to hold list of destinations for each train
  int** stationArray = new int*[numTrains];
  for(int i = 0; i < numTrains; i++){
    stationArray[i] = new int[maxStations];
  }

  // Array of mutexes to represent train lines. Essentially an adjacency
  // matrix where I assume all stations are adjacent.
  trainLineMutex = new std::mutex*[maxStations];
  for(int i = 0; i < maxStations; i++){
    trainLineMutex[i] = new std::mutex[maxStations];
  }

  int trainNum = 0;
  while(std::getline(inFile, line)){
    std::istringstream iss(line);
    iss >> queueLength[trainNum];
    for(int i = 0; i < queueLength[trainNum]; i++){
      iss >> stationArray[trainNum][i];
    }
    trainNum++;
  }

  std::thread** t = new std::thread*[numTrains];
  for(int i = 0; i < numTrains; i++){
    t[i] = new std::thread(choochoo, i, queueLength[i], stationArray[i]);
  }

  
  // Wait until this flag is switched
  chillForASec = false;

  for (int i = 0; i < numTrains; i++){
    t[i]->join();
  }

  std::cout << "\nAll trains have reached their destinations.\n";

  for(int i = 0; i < numTrains; i++){
    delete stationArray[i];
  }
  delete stationArray;
  for(int i = 0; i < maxStations; i++){
    delete trainLineMutex[i];
  }
  delete trainLineMutex;

  return 0;
}
