#include <mpi.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <vector>

#define FIELDSIZE 256

#define TEST true

// This array is only valid for rank 0. Don't expect rank1+ to have access.
int* dim = new int[2];
char*** csvData;

enum Mode { SR , BG };
enum Operation { MIN, MAX, AVG, NUMBER };

//static void performOperation(Mode mode, Operation op, receiveBuffer, int numRowsToWork);



static void do_rank_0_work_sr(int communicatorSize, int colIndex, int numRowsToWork, int numCols, int firstFieldTag, int secondFieldTag, int valueTag){

	MPI_Request sendReq;

	std::cout << "Sending data to other processes..." << std::endl;

  int* displs = (int*) malloc((FIELDSIZE+1)*sizeof(char)*(numCols));
  char receiveBuffer[(FIELDSIZE+1)*3];
  int receiveCount = numRowsToWork;

  MPI_Scatterv(
    &csvData[0][colIndex],
    &numRowsToWork,
    displs,
    MPI_CHAR,
    &receiveBuffer,
    receiveCount,
    MPI_CHAR,
    0,
    MPI_COMM_WORLD);
  

	std::cout << "Doing rank0 work..." << std::endl;
  char returnChars[(FIELDSIZE+1)*3];
  char receivedChars[communicatorSize][(FIELDSIZE+1)*3];

  Mode mode = SR;
  Operation op = MAX;

	std::cout << "Waiting for the others to send me their results..." << std::endl;

  MPI_Reduce(
    &receiveBuffer,
    &receivedChars,
    (FIELDSIZE+1)*3,
    MPI_CHAR,
    MPI_MAX,
    0,
    MPI_COMM_WORLD);



}


static void process(int rank, int communicatorSize, int argc, char** arguments, int* dimensions){
  std::string processString = "Executing query as ";
  if(arguments[1] == std::string("sr")){
    int colIndex;
    int numRowsToWork = 500/rank;
    int firstFieldTag = 0;
    int secondFieldTag = 1;
    int valueTag= 2;
    if(rank == 0){
      std::cout << processString << "Scatter-Reduce...\n";
      do_rank_0_work_sr(communicatorSize, colIndex, numRowsToWork, dimensions[1], firstFieldTag, secondFieldTag, valueTag);
    }

  }
  /*
  else if(arguments[1] == std::string("bg")){
    if(rank == 0){
      std::cout << processString << "Broadcast-Gather...\n";
      do_rank_0_work_bg(communicatorSize, argc, arguments, dimensions);
    }
    else{
      do_rank_n_work_bg(communicatorSize, argc, arguments);
    }

  }*/


}

int* readFile(std::string filename){

  int row = 0;
  int col = 0;
  {
    std::ifstream file(filename);
    std::string line;
    while(std::getline(file, line)){
      std::stringstream lineStream(line);
      std::string field;
      while(std::getline(lineStream, field,',')){
        col++;
      }
      row++;
    }
    row--;
    col--;
    
    
  }


  // Store dimensions
  dim[0] = row;
  dim[1] = col;
  if(TEST){
    std::cout << dim[0] << " " << dim[1] << "\n";
  }


  csvData = new char**[dim[0]];
  for(int i = 0; i < dim[0]; i++){
    csvData[i] = new char*[dim[1]];
    for(int j = 0; j < dim[0]; j++){
      csvData[i][j] = new char[FIELDSIZE+1];
    }
  }


  std::ifstream fileStream(filename);
  std::string line;
  int r = 0;
  int c = 0;
  
  while(std::getline(fileStream, line)){
    std::stringstream lineStream(line);
    std::string field;
    while(std::getline(lineStream, field,',')){

      sprintf(csvData[r][c], "%.4s", field.c_str());
      c++;
    }
    r++;
  }
  

  return dim;
}

int main (int argc, char **argv){

  MPI_Init(&argc, &argv);

	int rank, communicatorSize;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &communicatorSize);
  
  
  std::string inputFile = "500_Cities__City-level_Data__GIS_Friendly_Format_.csv";
  if(500 % communicatorSize != 0 && rank ==0){
    std::cout << "Work does not divide evenly. Bad!\n";
    return 0;
  }

  if(rank == 0){
    int* dimensions = readFile(inputFile);
  }

	if (argc < 4){
		if (rank == 0){
			std::cout << "At least 3 arguments will be required.\n";
    }
	}
	else {
  //  process(rank, communicatorSize, argc, argv, dimensions);
	}



  if(TEST){
    //std::cout << csvData[2][2] << "\n";
  }

  



	MPI_Finalize();
	return 0;
}
