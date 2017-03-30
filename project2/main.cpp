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

/*
 * Dustin Horvath
 * 3/29/17
 * openmpi project: reads a CSV file and allows querying via multiple mpi processes.
 * to use:
 * `make && mpirun -np 20 proj2 sr max/min/avg D`
 * `make && mpirun -np  5 proj2 sr number AS gt/lt 55`
 * "                  " 4 proj2 bg max/min D E I M`
 *
 */


// This array is only valid for rank 0. Don't expect rank1+ to have access.
int* dim = new int[2];
char* csvData;
char* headerData;

// These make switch statements a bit tidier
enum Mode { SR , BG };
enum Operation { MIN, MAX, AVG, NUMGT, NUMLT };

static void do_sr_work(int communicatorSize, int rank, int colIndex, int numRowsToWork, int numCols, Operation op, int bound){

	MPI_Request dataReq;

  if(rank == 0) std::cout << "Sending data to other processes..." << std::endl;

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

	if(rank == 0) std::cout << "Waiting for the others to send me their results...\n" << std::endl;
  
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


static void do_bg_work(int communicatorSize, int rank, int* cols, int numRows, int numCols, Operation op, int bound){

	MPI_Request dataReq;

  if(rank == 0) std::cout << "Sending data to other processes..." << std::endl;


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


	if(rank == 0) std::cout << "Waiting for the others to send me their results...\n" << std::endl;
  
  int currentRowIndex = 0;
  int valueRowIndex = 0;
  double value = strtol(&csvData[cols[rank]*(FIELDSIZE+1)*numRows],NULL,0);
  double sum = 0;
  switch(op){
    case MAX:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&csvData[cols[rank]*(FIELDSIZE+1)*numRows+(FIELDSIZE+1)*i],NULL,0);
      
        if(valueAtCurrentIndex > value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
    break;

    case MIN:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&csvData[cols[rank]*(FIELDSIZE+1)*numRows+(FIELDSIZE+1)*i],NULL,0);
      
        if(valueAtCurrentIndex < value){
          value = valueAtCurrentIndex;
          valueRowIndex = i;
        }
      }
    break;      
    
    case AVG:
      for(int i = 0; i < numRows; i++){
        double valueAtCurrentIndex = strtol(&csvData[cols[rank]*(FIELDSIZE+1)*numRows+(FIELDSIZE+1)*i],NULL,0);
        sum += valueAtCurrentIndex;
      }
      value = sum/numRows;
      valueRowIndex = 0;
    break;
  }

  double* sendBuffer = new double[2];
  double* receiveBuffer = new double[communicatorSize*2];

  // Contains value that will be sent back to rank 0
  sendBuffer[0] = value;
  sendBuffer[1] = valueRowIndex;

  MPI_Gather(
    sendBuffer,
    2,
    MPI_DOUBLE,
    receiveBuffer,
    2,
    MPI_DOUBLE,
    0,
    MPI_COMM_WORLD);

  if(rank==0) {
    char stringBuffer[FIELDSIZE+1];
    char stateBuffer[FIELDSIZE+1];
    char cityBuffer[FIELDSIZE+1];
    std::cout << std::fixed;
    std::cout << std::setprecision(2);
    std::string operation;

    switch(op){
      
      case MAX:
        operation = "max ";
      case MIN:
        if(op == MIN) operation = "min ";
      case AVG:
        if(op == AVG) operation = "avg ";
        for(int i = 0; i < communicatorSize; i++){
          // Messy, but w/e. My string handling in C isn't awesome
          memcpy(stringBuffer, &headerData[cols[i]*(FIELDSIZE+1)], FIELDSIZE);
          stringBuffer[FIELDSIZE] = '\0';
          memcpy(stateBuffer, &csvData[ (int)receiveBuffer[2*i+1] * (FIELDSIZE+1)], FIELDSIZE);
          stateBuffer[FIELDSIZE] = '\0';
          memcpy(cityBuffer, &csvData[ (int)receiveBuffer[2*i+1] * (FIELDSIZE+1) + numRows*(FIELDSIZE+1)], FIELDSIZE);
          cityBuffer[FIELDSIZE] = '\0';

          std::cout <<  operation <<
                        std::string(stringBuffer) << 
                        " = " <<
                        receiveBuffer[2*i] <<
                        "; " <<
                        std::string(cityBuffer) <<
                        ", " <<
                        std::string(stateBuffer) <<
                        "\n";
        }
      break;
    }
  }
}

// Looks at parameters and processes the call either as SR or BG
static void process(int rank, int communicatorSize, int argc, char** argv, Mode mode, Operation op, int bound, int* dimensions){

  std::string processString = "Executing query as ";
  if(mode == SR){
    // Compute column index
    int colIndex = 0;
    if(strlen(argv[3]) == 1){
      colIndex += (int)argv[3][0] - (int)'A';
    }
    else if(strlen(argv[3]) == 2){
      colIndex += (int)argv[3][0] - (int)'A';
      colIndex += (int)argv[3][1] - (int)'A';
    }

    // This value is the number of rows each process will be responsible for
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

    // Compute column indices
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

    do_bg_work(communicatorSize, rank, cols, numRows, dimensions[1], op, bound);

  }
}


// Takes a filename, stores all the data in csvData and returns an int[2] that
// contains [rows][columns]
int* readFile(std::string filename){



  int row = 0;
  int col = 0;
  int count = 0;
  // Wrapping this so that my file streams close as soon as they're not needed
  //
  // This block is only here to count the rows and columns
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

  // csvData holds all of the csv file minus the header. It's the size of all
  // the fields in the file X the "fieldsize", chosen somewhat arbitrarily
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
        // While the file is read one row at a time, I'm writing it here
        // column-wise. This will later allow mpi_scatter to traverse the
        // contiguous chars much more easily.
        strncpy(&csvData[ c*(dim[0]-1)*(FIELDSIZE+1) + (FIELDSIZE+1)*r ], field.c_str(), FIELDSIZE);
        c++;
      }
      
      r++;

    }

  }

  // Return the file dimensions to the caller
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
