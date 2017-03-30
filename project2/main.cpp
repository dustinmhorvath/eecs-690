#include <mpi.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <iomanip>

#include <boost/tokenizer.hpp>

#define FIELDSIZE 256

#define TEST true

// This array is only valid for rank 0. Don't expect rank1+ to have access.
int* dim = new int[2];
char* csvData;
char* headerData;

enum Mode { SR , BG };
enum Operation { MIN, MAX, AVG, NUMGT, NUMLT };

//static void performOperation(Mode mode, Operation op, receiveBuffer, int numRowsToWork);



static void do_sr_work(int communicatorSize, int rank, int colIndex, int numRowsToWork, int numCols, Operation op, int bound){

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
  
  MPI_Op reduceOperation;
  int currentRowIndex = 0;
  int valueRowIndex = 0;
  double value = strtol(&receiveBuffer[0],NULL,0);
  double sum = 0;
  switch(op){
    case MAX:
      for(int i = 0; i < numRowsToWork; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);

        if(valueAtCurrentIndex > value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
      reduceOperation = MPI_MAX;

    break;
    
    case MIN:
      for(int i = 0; i < numRowsToWork; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);

        if(valueAtCurrentIndex < value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
      reduceOperation = MPI_MIN;
    break;
    
    case AVG:
      for(int i = 0; i < numRowsToWork; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);
        sum += valueAtCurrentIndex;
      }
      value = sum/numRowsToWork;
      reduceOperation = MPI_SUM;
    break;
    
    case NUMGT:
      for(int i = 0; i < numRowsToWork; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);
        if(valueAtCurrentIndex > bound){
          sum++;
        }
      }
      reduceOperation = MPI_SUM;
      value = sum;
    break;
    
    case NUMLT:
      for(int i = 0; i < numRowsToWork; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);
        if(valueAtCurrentIndex < bound){
          sum++;
        }
      }
      reduceOperation = MPI_SUM;
      value = sum;
    break;

  }


  double* sendBuffer = new double[1];
  double* receiveDoubleBuffer = new double[1];

  // Contains value that will be sent back to rank 0
  sendBuffer[0] = value;

  // This seems to return whatever sendBuffer contains the max/min/etc value
  MPI_Reduce(
    sendBuffer,
    receiveDoubleBuffer,
    1,
    MPI_DOUBLE,
    reduceOperation,
    0,
    MPI_COMM_WORLD);

  if(rank==0) {
    switch(op){
      char stringBuffer[FIELDSIZE+1];

      case MAX:
      case MIN:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
                std::cout <<  std::string(stringBuffer) << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
      case AVG:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';

        std::cout <<  "Average " << 
                      std::string(stringBuffer) << 
                      " = " <<
                      receiveDoubleBuffer[0]/communicatorSize <<
                      "\n";
      break;

      case NUMGT:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout <<  "Number cities with " <<
                      std::string(stringBuffer) << 
                      " gt " <<
                      bound << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
      case NUMLT: 
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout <<  "Number cities with " <<
                      std::string(stringBuffer) << 
                      " lt " <<
                      bound << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
    }

  }



}


static void do_rank_0_work_bg(int communicatorSize, int rank, int* cols, int numRows, int numCols, Operation op, int bound){

	MPI_Request dataReq;

  if(rank == 0) std::cout << "Sending data to other processes..." << std::endl;


  char* receiveBuffer = new char[(FIELDSIZE+1)*numRows*numCols];
  int sendCount = (FIELDSIZE+1)*numRows*numCols;

  // rank 0 *already* has csvData(global), but it's not defined for the other
  // ranks.
  if(rank!=0){
    csvData = new char[numRows*numCols*(FIELDSIZE+1)];
  }

  {
    MPI_Ibcast(
      &csvData[0],
      sendCount,
      MPI_CHAR,
      0,
      MPI_COMM_WORLD,
      &dataReq);
  }

  MPI_Status dataStatus;
  if(rank!=0) MPI_Waitall(1, &dataReq, &dataStatus);

/*
  for(int i = 0; i < 10; i++){
    std::cout << "I am rank " << rank << " and I have " << &csvData[(FIELDSIZE+1)*i*numRows] << " as " << i << "th element.\n";
  }
*/



	if(rank == 0) std::cout << "Doing rank0 work..." << std::endl;

	if(rank == 0) std::cout << "Waiting for the others to send me their results..." << std::endl;

  
  
  int currentRowIndex = 0;
  int valueRowIndex = 0;
  //TODO change 0 address to start of row
  double value = strtol(&csvData[0],NULL,0);
  double sum = 0;
  switch(op){
    case MAX:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&csvData[cols[rank]*(FIELDSIZE+1)*numRows],NULL,0);
      
        if(valueAtCurrentIndex > value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
      std::cout << std::fixed;
      std::cout << std::setprecision(2);
      std::cout << value << "\n";

    break;
    /*
    case MIN:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);

        if(valueAtCurrentIndex < value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
    break;
    
    case AVG:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&receiveBuffer[(FIELDSIZE+1)*i],NULL,0);
        sum += valueAtCurrentIndex;
      }
      value = sum/numRows;
    break;
*/


    
    
  }


  double* sendBuffer = new double[1];
  double* receiveDoubleBuffer = new double[1];

  // Contains value that will be sent back to rank 0
  sendBuffer[0] = value;

 /* 
  if(rank==0) {
    switch(op){
      char stringBuffer[FIELDSIZE+1];

      case MAX:
      case MIN:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
                std::cout <<  std::string(stringBuffer) << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
      case AVG:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';

        std::cout <<  "Average " << 
                      std::string(stringBuffer) << 
                      " = " <<
                      receiveDoubleBuffer[0]/communicatorSize <<
                      "\n";
      break;

      case NUMGT:
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout <<  "Number cities with " <<
                      std::string(stringBuffer) << 
                      " gt " <<
                      bound << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
      case NUMLT: 
        std::cout << std::fixed;
        std::cout << std::setprecision(2);
        memcpy(stringBuffer, &headerData[(FIELDSIZE+1)*colIndex], FIELDSIZE);
        stringBuffer[FIELDSIZE] = '\0';
        std::cout <<  "Number cities with " <<
                      std::string(stringBuffer) << 
                      " lt " <<
                      bound << 
                      " = " <<
                      receiveDoubleBuffer[0] <<
                      "\n";
      break;
    }

  }
  */



}


static void process(int rank, int communicatorSize, int argc, char** argv, Mode mode, Operation op, int bound, int* dimensions){



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

    

    int numRowsToWork = (dimensions[0]-1)/communicatorSize;

    //TODO get this column index dynamically
    
    if(rank == 0) std::cout << processString << "Scatter-Reduce...\n";
    do_sr_work(communicatorSize, rank, colIndex, numRowsToWork, dimensions[1], op, bound);

  }
 
  
  else if(mode = BG){

    // Index all of the column names
    int numCols = argc-3;
    // Catch incorrect number of ranks
    if(numCols != communicatorSize){
      if(rank==0) std::cout << "Number of processes (" << communicatorSize << ") != number of columns (" << numCols << "). Exiting...\n";
      return;
    }

    int* cols = new int[numCols];
    for(int i = 0; i < argc-3; i++){
      cols[i] = 0;
      if(strlen(argv[3+i]) == 1){
        cols[i] += (int)argv[3+i][0] - (int)'A';
      }
      else if(strlen(argv[3+i]) == 2){
        cols[i] += (int)argv[3+i][0] - (int)'A';
        cols[i] += (int)argv[3+i][1] - (int)'A';
      }
    }


    int numRows = dimensions[0]-1;

    if(rank==0) std::cout << processString << "Broadcast-Gather...\n";

    do_rank_0_work_bg(communicatorSize, rank, cols, numRows, dimensions[1], op, bound);

  }


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
  headerData = new char[(dim[0]-1)*(FIELDSIZE+1)];

  int r = 0;
  int c = 0;

  {
    std::ifstream infile( filename );
    std::string line;
    
    //Consume header line
    getline( infile, line );
    
    {
      boost::escaped_list_separator<char> sep("\\", ",", "\"");
      boost::tokenizer<boost::escaped_list_separator<char>> tok(line, sep);
      c=0;
      
      for(boost::tokenizer<boost::escaped_list_separator<char>>::iterator beg=tok.begin(); beg!=tok.end();++beg){
        std::string field = *beg;
        //std::cout << field << "\n";
        strncpy(&headerData[ c*(FIELDSIZE+1)], field.c_str(), FIELDSIZE);
        c++;
      }
      

    }
    


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

  int bound = 0;
  if(std::string(argv[2]) == "max"){
    op = MAX;
  }
  else if(std::string(argv[2]) == "min"){
    op = MIN;
  }  
  else if(std::string(argv[2]) == "avg"){
    op = AVG;
  }
  else if(std::string(argv[2]) == "number" && std::string(argv[4]) == "gt"){
    op = NUMGT;
    bound = std::stoi(std::string(argv[5]));
  }
  else if(std::string(argv[2]) == "number" && std::string(argv[4]) == "lt"){
    op = NUMLT;
    bound = std::stoi(std::string(argv[5]));
  }

  else{
    std::cout << "Invalid operation " << argv[2] << "\n";
    MPI_Finalize();
    return 0;
  }



  process(rank, communicatorSize, argc, argv, mode, op, bound, dimensions);

  /*
  if(rank == 0){
    for(int i = 0; i < dim[0]-1; i++){
      std::cout << i << " " << &csvData[(FIELDSIZE+1)*i+(FIELDSIZE+1)*(dim[0]-1)*4] << "\n";
    }
  }
  */

  /*
  if(rank == 0){
    for(int i = 0; i < dim[1]; i++){
      std::cout << i << " " << &headerData[(FIELDSIZE+1)*i] << "\n";
    }
  } 
  */
  


	MPI_Finalize();
	return 0;
}
