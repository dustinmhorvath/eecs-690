#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include "barrier.h"

std::mutex coutMutex;
std::mutex** trainLineMutex;
bool chillForASec = true;
Barrier b;
// Total trains, which I want accessible to threads.
int numTrains;

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
  while(currentStationIndex != numStations-1){
    // Verbose for sanity's sake
    int currentStation = stationList[currentStationIndex];
    int nextStation = stationList[currentStationIndex+1];
    // Lock on track required. I'm using the notation that each track is
    //  [lowernumberstation][highernumberstation], regardless of travelling
    //  direction.
    std::unique_lock<std::mutex> trackLock(trainLineMutex[std::min(currentStation, nextStation)][std::max(currentStation, nextStation)], std::defer_lock);

    if(trackLock.try_lock()){

      coutMutex.lock();      
      std::cout << "I am train " << trainNumber << " moving from " << currentStation << " to " << nextStation 
        << " locking [" << std::min(currentStation, nextStation) << "][" 
        << std::max(currentStation, nextStation) << "]\n";
      coutMutex.unlock();
      // Unlock after done
      trackLock.unlock();

      currentStationIndex++;
      
    }
    else{
      coutMutex.lock();
      std::cout << "I am train " << trainNumber << " moving from " << currentStation << " to " << nextStation 
        << " waiting for [" << std::min(currentStation, nextStation) << "][" 
        << std::max(currentStation, nextStation) << "]\n";
      coutMutex.unlock();
      
    }

    b.barrier(numTrains);
    b.barrier(numTrains);
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
  // Total station count
  int maxStations;
  std::getline(inFile, line);
  std::istringstream issQuick(line);
  issQuick >> numTrains;
  issQuick >> maxStations;
  std::cout << numTrains << " " << maxStations << "\n";

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
    std::cout << queueLength[trainNum] << " ";
    for(int i = 0; i < queueLength[trainNum]; i++){
      iss >> stationArray[trainNum][i];
      std::cout << stationArray[trainNum][i] << " ";
    }
    std::cout << "\n";
    trainNum++;
  }

  std::thread** t = new std::thread*[numTrains];
  for(int i = 0; i < numTrains; i++){
    t[i] = new std::thread(choochoo, i, queueLength[i], stationArray[i]);
  }

  
  // Wait until this flag is switched
  chillForASec = false;

  //coutMutex.lock();
  //std::cout << "I'm HQ and we've got trains.\n";
  //coutMutex.unlock();

  for (int i = 0; i < numTrains; i++){
    t[i]->join();
  }

  return 0;
}
