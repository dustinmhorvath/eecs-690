#include <mpi.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <vector>

#include <boost/tokenizer.hpp>

#define FIELDSIZE 256

#define TEST true

// This array is only valid for rank 0. Don't expect rank1+ to have access.
int* dim = new int[2];
char* csvData;

enum Mode { SR , BG };
enum Operation { MIN, MAX, AVG, NUMBER };

//static void performOperation(Mode mode, Operation op, receiveBuffer, int numRowsToWork);



static void do_rank_0_work_sr(int communicatorSize, int rank, int colIndex, int numRowsToWork, int numCols, Operation op, int firstFieldTag, int secondFieldTag, int valueTag){

	MPI_Request dataReq;

  if(rank == 0) std::cout << "Sending data to other processes..." << std::endl;

  if(rank==1 && TEST){
    std::cout << "I am rank 1 with numRowsToWork=" << numRowsToWork << " and numCols=" << numCols << " and colIndex=" << colIndex << "\n";
  }

  char* receiveBuffer = new char[(FIELDSIZE+1)*numRowsToWork];
  int sendCount = (FIELDSIZE+1)*numRowsToWork;


  MPI_Iscatter(
    &csvData[colIndex*(FIELDSIZE+1)*(numRowsToWork*communicatorSize)],
    sendCount,
    MPI_CHAR,
    receiveBuffer,
    sendCount,
    MPI_CHAR,
    0,
    MPI_COMM_WORLD,
    &dataReq);



  MPI_Status dataStatus;
  if(rank!=0) MPI_Waitall(1, &dataReq, &dataStatus);

	if(rank == 0) std::cout << "Doing rank0 work..." << std::endl;

	if(rank == 0) std::cout << "Waiting for the others to send me their results..." << std::endl;

/*
  for(int i = 1; i < numRowsToWork; i++){
    std::cout << "I am rank " << rank << " and I have " << &receiveBuffer[(FIELDSIZE+1)*i] << " as " << i << "th element.\n";
  }
*/
  int currentRowIndex = 0;
  int valueRowIndex = 0;
  int value = strtol(&receiveBuffer[0],NULL,0);
  switch(op){
    case MAX:
      for(int i = 0; i < numRowsToWork; i++){
        char stringBuffer[FIELDSIZE+1];
        memcpy(stringBuffer, &receiveBuffer[(FIELDSIZE+1)*i], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout << stringBuffer << "\n";

        int valueAtCurrentIndex = 0;//std::stoi(stringBuffer);

        //std::cout << valueAtCurrentIndex << "\n";

        if(valueAtCurrentIndex > value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
      break;
    case MIN:
      for(int i = 0; i < numRowsToWork; i++){
        char stringBuffer[FIELDSIZE+1];
        memcpy(stringBuffer, &receiveBuffer[(FIELDSIZE+1)*i], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout << stringBuffer << "\n";

        int valueAtCurrentIndex = value;

        if(valueAtCurrentIndex < value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
      break;
    case AVG:
    int sum = 0;
      for(int i = 0; i < numRowsToWork; i++){
        int valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);
        sum += valueAtCurrentIndex;

      }
      value = sum/numRowsToWork;
      break;

    break;


  }
  int* sendBuffer = new int[1];
  int* receiveIntBuffer = new int[1];

  // Sending:
  // RANK
  // VALUE
  // ROW INDEX OF VALUE
  sendBuffer[0] = value;


  // This seems to return whatever sendBuffer contains the max/min/etc value
  MPI_Reduce(
    sendBuffer,
    receiveIntBuffer,
    3,
    MPI_INT,
    MPI_MIN,
    0,
    MPI_COMM_WORLD);

  if(rank==0) std::cout << "Rank " << rank << " has finished with " << receiveIntBuffer[0] << "\n";



}


static void process(int rank, int communicatorSize, int argc, char** argv, Mode mode, Operation op, int* dimensions){



  std::string processString = "Executing query as ";
  if(mode == SR){
    int colIndex = 0;
    if(strlen(argv[3]) == 1){
      colIndex += (int)argv[3][0] - (int)'A';
    }
    else if(strlen(argv[3]) == 2){
      colIndex += (int)argv[3][0] - (int)'A';
      colIndex += (int)argv[3][1] - (int)'A';
    }



    //TODO get this column index dynamically
    int numRowsToWork = (dimensions[0]-1)/communicatorSize;
    int firstFieldTag = 0;
    int secondFieldTag = 1;
    int valueTag= 2;
    
    if(rank == 0) std::cout << processString << "Scatter-Reduce...\n";
    do_rank_0_work_sr(communicatorSize, rank, colIndex, numRowsToWork, dimensions[1], op, firstFieldTag, secondFieldTag, valueTag);

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


// Takes a filename, stores all the data in csvData and returns an int[2] that
// contains [rows][columns]
int* readFile(std::string filename){



  int row = 0;
  int col = 0;
  int count = 0;
  {
    std::ifstream infile( filename );
    std::string line;
    while(infile){
      if (!getline( infile, line )) break;

      boost::escaped_list_separator<char> sep("\\", ",", "\"");
      boost::tokenizer<boost::escaped_list_separator<char>> tok(line, sep);
      for(boost::tokenizer<boost::escaped_list_separator<char>>::iterator beg=tok.begin(); beg!=tok.end();++beg){
        count++;
      }
      col = count;
      count = 0;
      row++;

    }
  
  }


  // Store dimensions
  dim[0] = row;
  dim[1] = col;

  csvData = new char[(dim[0]-1)*dim[1]*(FIELDSIZE+1)];

  int r = 0;
  int c = 0;

  {
    std::ifstream infile( filename );
    std::string line;
    // TODO consuming column line for now
    getline( infile, line );

    while(infile){
      if (!getline( infile, line )) break;

      boost::escaped_list_separator<char> sep("\\", ",", "\"");
      boost::tokenizer<boost::escaped_list_separator<char>> tok(line, sep);
      c=0;
      for(boost::tokenizer<boost::escaped_list_separator<char>>::iterator beg=tok.begin(); beg!=tok.end();++beg){
        std::string field = *beg;
        //std::cout << field << "\n";
        strncpy(&csvData[ c*(dim[0]-1)*(FIELDSIZE+1) + (FIELDSIZE+1)*r ], field.c_str(), FIELDSIZE);
        c++;
      }
      
      r++;

    }

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

  int* dimensions = readFile(inputFile);


  if (argc < 4){
		if (rank == 0){
			std::cout << "At least 3 arguments will be required.\n";
      MPI_Finalize();
      return 0;
    }
	}

  
  Mode mode;
  Operation op;
  
  if(std::string(argv[1]) == "sr"){
    mode = SR;
  }
  else if(std::string(argv[1]) == "bg"){
    mode = BG;
  }
  else{
    std::cout << "Invalid mode " << argv[1] << "\n";
    MPI_Finalize();
    return 0;
  }


  if(std::string(argv[2]) == "max"){
    op = MAX;
  }
  else if(std::string(argv[2]) == "min"){
    op = MIN;
  }  
  else if(std::string(argv[2]) == "avg"){
    op = AVG;
  }
  else if(std::string(argv[2]) == "number"){
    op = NUMBER;
  }
  else{
    std::cout << "Invalid operation " << argv[2] << "\n";
    MPI_Finalize();
    return 0;
  }



  process(rank, communicatorSize, argc, argv, mode, op, dimensions);

  /*
  if(rank == 0){
    for(int i = 0; i < dim[0]-1; i++){
      std::cout << i << " " << &csvData[(FIELDSIZE+1)*i+(FIELDSIZE+1)*(dim[0]-1)*4] << "\n";
    }
  }
  */
  


	MPI_Finalize();
	return 0;
}
