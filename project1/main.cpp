#include <mutex>
#include <thread>
#include <iostream>
#include <queue>
#include <fstream>
#include <string>
#include <sstream>



std::mutex coutMutex;

void choochoo(int trainNumber, int numStations, int* stationList)
{
  coutMutex.lock();
  std::cout << "I am train " << trainNumber << " and my stationlist is ";
  for(int i = 0; i < numStations; i++){
    std::cout << stationList[i] << " ";
  }
  std::cout << "\n";
  coutMutex.unlock();
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

  // Total trains
  int numTrains;
  // Array holding length of route for each train
  int queueLength[numTrains];
  // Total station count
  int maxStations;
  std::getline(inFile, line);
  std::istringstream issQuick(line);
  issQuick >> numTrains;
  issQuick >> maxStations;
  std::cout << numTrains << " " << maxStations << "\n";

  std::thread** t = new std::thread*[numTrains];
  int** stationArray = new int*[numTrains];
  for(int i = 0; i < numTrains; i++){
    stationArray[i] = new int[maxStations];
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


  for (int i = 0; i < numTrains; i++){
    t[i] = new std::thread(choochoo, i, queueLength[i], stationArray[i]);
  }

  coutMutex.lock();
  std::cout << "I'm HQ and we've got trains.\n";
  coutMutex.unlock();

  for (int i = 0; i < numTrains; i++){
    t[i]->join();
  }

  return 0;
}
