// ContourGenerator.c++ - Code to read a scalar data field and produce
// contours at requested levels.

#include "ContourGenerator.h"

#include <stdio.h>
#include <cstdlib>

// A common utility extracted from the source code associated with
// the book "Heterogeneous Computing with OpenCL".

// This function reads in a text file and stores it as a char pointer
const char* readSource(const char* kernelPath) {

   FILE *fp;
   char *source;
   long int size;

   printf("Program file is: %s\n", kernelPath);

   fp = fopen(kernelPath, "rb");
   if(!fp) {
      printf("Could not open kernel file\n");
      exit(-1);
   }
   int status = fseek(fp, 0, SEEK_END);
   if(status != 0) {
      printf("Error seeking to end of file\n");
      exit(-1);
   }
   size = ftell(fp);
   if(size < 0) {
      printf("Error getting file position\n");
      exit(-1);
   }

   rewind(fp);

   source = (char *)malloc(size + 1);

   int i;
   for (i = 0; i < size+1; i++) {
      source[i]='\0';
   }

   if(source == NULL) {
      printf("Error allocating space for the kernel source\n");
      exit(-1);
   }

   fread(source, 1, size, fp);
   source[size] = '\0';

	//printf("Returning file:\n%s\n", source);

   return source;
}


// A couple simple utility functions:
bool debug = false;
void checkStatus(std::string where, cl_int status, bool abortOnError)
{
	if (debug || (status != 0))
		std::cout << "Step " << where << ", status = " << status << '\n';
	if ((status != 0) && abortOnError)
		exit(1);
}

void reportVersion(cl_platform_id platform)
{
	// Get the version of OpenCL supported on this platform
	size_t strLength;
	clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 0, nullptr, &strLength);
	char* version = new char[strLength+1];
	clGetPlatformInfo(platform, CL_PLATFORM_VERSION, strLength+1, version, &strLength);
	//std::cout << version << '\n';
	delete [] version;
}

void showProgramBuildLog(cl_program pgm, cl_device_id dev)
{
	size_t size;
	clGetProgramBuildInfo(pgm, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &size);
	char* log = new char[size+1];
	clGetProgramBuildInfo(pgm, dev, CL_PROGRAM_BUILD_LOG, size+1, log, nullptr);
	std::cout << "LOG:\n" << log << "\n\n";
	delete [] log;
}

// Typical OpenCL startup

// Some global state variables (These would be better packaged as
// instance variables of some class.)
// 1) Platforms
cl_uint numPlatforms = 0;
cl_platform_id* platforms = nullptr;
cl_platform_id curPlatform;
// 2) Devices
cl_uint numDevices = 0;
cl_device_id* devices = nullptr;

void typicalOpenCLProlog(cl_device_type desiredDeviceType)
{
	//-----------------------------------------------------
	// Discover and query the platforms
	//-----------------------------------------------------

	cl_int status = clGetPlatformIDs(0, nullptr, &numPlatforms);
	checkStatus("clGetPlatformIDs-0", status, true);
 
	platforms = new cl_platform_id[numPlatforms];
 
	status = clGetPlatformIDs(numPlatforms, platforms, nullptr);
	checkStatus("clGetPlatformIDs-1", status, true);
	curPlatform = platforms[0];

	reportVersion(curPlatform);

	//-------------------------------------------------------------------------------
	// Discover and initialize the devices on the selected (current) platform
	//-------------------------------------------------------------------------------

	status = clGetDeviceIDs(curPlatform, desiredDeviceType, 0, nullptr, &numDevices);
	checkStatus("clGetDeviceIDs-0", status, true);

	devices = new cl_device_id[numDevices];

	status = clGetDeviceIDs(curPlatform, desiredDeviceType, numDevices, devices, nullptr);
	checkStatus("clGetDeviceIDs-1", status, true);
}



ContourGenerator::ContourGenerator(std::istream& inp) :
	vertexValues(nullptr)
{

	inp >> nRowsOfVertices >> nColsOfVertices;
	std::string scalarDataFieldFileName;
	inp >> scalarDataFieldFileName;
	std::ifstream scalarFieldFile(scalarDataFieldFileName.c_str());
	if (scalarFieldFile.good())
	{
		readData(scalarFieldFile);
		scalarFieldFile.close();
	}
	else
	{
		std::cerr << "Could not open " << scalarDataFieldFileName
		          << " for reading.\n";
		nRowsOfVertices = nColsOfVertices = 0;
	}
}

ContourGenerator::~ContourGenerator()
{
	if (vertexValues != nullptr)
		delete [] vertexValues;
	// Delete any GPU structures (buffers) associated with this model as well!
}

int ContourGenerator::computeContourEdgesFor(float level, vec2*& lines)
{
  int numDimsToUse = 1;

	typicalOpenCLProlog(CL_DEVICE_TYPE_DEFAULT);
	//-------------------------------------------------------------------
	// Create a context for all or some of the discovered devices
	//         (Here we are including all devices.)
	//-------------------------------------------------------------------

	cl_int status;
	cl_context context = clCreateContext(nullptr, numDevices, devices,
		nullptr, nullptr, &status);
	checkStatus("clCreateContext", status, true);

	//-------------------------------------------------------------
	// Create a command queue for ONE device in the context
	//         (There is one queue per device per context.)
	//-------------------------------------------------------------

	cl_command_queue cmdQueue = clCreateCommandQueue(context, devices[0], 
		0, &status);
	checkStatus("clCreateCommandQueue", status, true);

	//----------------------------------------------------------
	// Create device buffers associated with the context
	//----------------------------------------------------------
  int numElements = nRowsOfVertices * nColsOfVertices;
	int numEdges = 0;

	size_t datasize = numElements*sizeof(float);
	std::cout << "DATASIZE = " << datasize << "\n";

	cl_mem d_vertexBuffer = clCreateBuffer( // Input array on the device
		context, CL_MEM_READ_ONLY, datasize, nullptr, &status);
	checkStatus("clCreateBuffer-A", status, true);

  cl_mem d_returnValue = clCreateBuffer( // Output array on the device
    context, CL_MEM_WRITE_ONLY, sizeof(int), nullptr, &status);
  checkStatus("clCreateBuffer-B", status, true);

  status = clEnqueueWriteBuffer(cmdQueue, 
		d_vertexBuffer, CL_FALSE, 0, datasize,                         
		vertexValues, 0, nullptr, nullptr);
	checkStatus("clEnqueueWriteBuffer-A", status, true);

  status = clEnqueueWriteBuffer(cmdQueue, 
		d_returnValue, CL_FALSE, 0, sizeof(int),
		&numEdges, 0, nullptr, nullptr);
	checkStatus("clEnqueueWriteBuffer-B", status, true);


	//-----------------------------------------------------
	// Create, compile, and link the program
	//----------------------------------------------------- 

	const char* firstProgramSource[] = { readSource("CountEdges.cl") };
	cl_program program = clCreateProgramWithSource(context, 
		1, firstProgramSource, nullptr, &status);
	checkStatus("clCreateProgramWithSource", status, true);

	status = clBuildProgram(program, numDevices, devices, 
		nullptr, nullptr, nullptr);
	checkStatus("clBuildProgram", status, false);

if (status == CL_BUILD_PROGRAM_FAILURE) {
    // Determine the size of the log
    size_t log_size;
    clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

    // Allocate memory for the log
    char *log = (char *) malloc(log_size);

    // Get the log
    clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

    // Print the log
    printf("%s\n", log);
    exit;
}

	//-----------------------------------------------------------
	// Create a kernel from one of the __kernel functions
	//         in the source that was built.
	//-----------------------------------------------------------

	cl_kernel kernel = clCreateKernel(program, "countEdges", &status);

	//-----------------------------------------------------
	// Set the kernel arguments
	//----------------------------------------------------- 

	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_vertexBuffer);
	checkStatus("clSetKernelArg-A", status, true);
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_returnValue);
	checkStatus("clSetKernelArg-B", status, true);
	status = clSetKernelArg(kernel, 2, sizeof(int), &nRowsOfVertices);
	checkStatus("clSetKernelArg-C", status, true);
	status = clSetKernelArg(kernel, 3, sizeof(int), &nColsOfVertices);
	checkStatus("clSetKernelArg-D", status, true);
  status = clSetKernelArg(kernel, 4, sizeof(float), &level);
	checkStatus("clSetKernelArg-E", status, true);

	
	//-----------------------------------------------------
	// Configure the work-item structure
	//----------------------------------------------------- 

  // TODO: too many kernels for smaller maps and not enough for big
	size_t localWorkSize[] = { 16, 16 };    
	size_t globalWorkSize[] = { 256 * 256 };
	if (numDimsToUse == 1)
		localWorkSize[0] = 32;
	else if (numDimsToUse == 2)
		localWorkSize[0] = localWorkSize[1] = 16;

	//-----------------------------------------------------
	// Enqueue the kernel for execution
	//----------------------------------------------------- 

	status = clEnqueueNDRangeKernel(cmdQueue, kernel,
		numDimsToUse,
		nullptr, globalWorkSize, // globalOffset, globalSize
		localWorkSize,
		0, nullptr, nullptr); // event information, if needed
	checkStatus("clEnqueueNDRangeKernel", status, true);

	//-----------------------------------------------------
	// Read the output buffer back to the host
	//----------------------------------------------------- 

	clEnqueueReadBuffer(cmdQueue, 
		d_returnValue, CL_TRUE, 0, sizeof(int), 
		&numEdges, 0, nullptr, nullptr);

	// block until all commands have finished execution

	clFinish(cmdQueue);

	//-----------------------------------------------------
	// Release OpenCL resources
	//----------------------------------------------------- 

  std::cout << "NUMEXPECTEDEDGES = " << numEdges << "\n";

	// Free OpenCL resources
  // Here, we're only dropping the things relevant to the counting kernel
	clReleaseKernel(kernel);
	clReleaseProgram(program);
  //I think we can reuse this vertex buffer.
  //clReleaseMemObject(d_vertexBuffer);
  clReleaseMemObject(d_returnValue);

  //////////////////////////////////////////////////////////////////////////
  //------------------ END COUNTING KERNEL -------------------------------//
  //////////////////////////////////////////////////////////////////////////

  // Fire a kernel to determine expected number of edges at the given "level'
	int numExpectedEdges = numEdges;

	// Create space for the line end points on the device
	int numExpectedPoints = 2 * numExpectedEdges; // each edge is: (x,y), (x,y)
  int expectedNumCoordValues = numExpectedPoints * 2;


  //////////////////////////////////////////////////////////////////////////
  //------------------ START COMPUTE KERNEL ------------------------------//
  //////////////////////////////////////////////////////////////////////////
  
  //----------------------------------------------------------
	// Create device buffers associated with the context
	//----------------------------------------------------------
  
  
  // Fire a kernel to compute the edge end points (determimes "numActualEdges")
	int numActualEdges = 0;

	size_t lineBufferSize = expectedNumCoordValues*sizeof(float);
  int computeKernelEdgeCount = 0;
  int loc = 0;

  cl_mem d_lineBuffer = clCreateBuffer( // Output array on the device
    context, CL_MEM_WRITE_ONLY, lineBufferSize, nullptr, &status);
  checkStatus("clCreateBuffer-A", status, true);

  cl_mem d_computeKernelEdgeCount = clCreateBuffer( // Output array on the device
    context, CL_MEM_READ_WRITE, sizeof(int), nullptr, &status);
  checkStatus("clCreateBuffer-B", status, true);

  cl_mem d_loc = clCreateBuffer( // Output array on the device
    context, CL_MEM_READ_WRITE, sizeof(int), nullptr, &status);
  checkStatus("clCreateBuffer-C", status, true);

  // write values to buffers

  status = clEnqueueWriteBuffer(cmdQueue, 
		d_computeKernelEdgeCount, CL_FALSE, 0, sizeof(int),
		&numActualEdges, 0, nullptr, nullptr);
	checkStatus("clEnqueueWriteBuffer-A", status, true);

  status = clEnqueueWriteBuffer(cmdQueue, 
		d_loc, CL_FALSE, 0, sizeof(int),
		&loc, 0, nullptr, nullptr);
	checkStatus("clEnqueueWriteBuffer-B", status, true);


	//-----------------------------------------------------
	// Create, compile, and link the program
	//----------------------------------------------------- 

  const char* secondProgramSource[] = { readSource("ComputeEdges.cl") };
	program = clCreateProgramWithSource(context, 
		1, secondProgramSource, nullptr, &status);
	checkStatus("clCreateProgramWithSource", status, true);

	status = clBuildProgram(program, numDevices, devices, 
		nullptr, nullptr, nullptr);
	checkStatus("clBuildProgram", status, false);

  if (status == CL_BUILD_PROGRAM_FAILURE) {
    // Determine the size of the log
    size_t log_size;
    clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

    // Allocate memory for the log
    char *log = (char *) malloc(log_size);

    // Get the log
    clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

    // Print the log
    printf("%s\n", log);
    exit;
  }

	//-----------------------------------------------------------
	// Create a kernel from one of the __kernel functions
	//         in the source that was built.
	//-----------------------------------------------------------

	kernel = clCreateKernel(program, "computeEdges", &status);

	//-----------------------------------------------------
	// Set the kernel arguments
	//----------------------------------------------------- 

	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_vertexBuffer);
	checkStatus("clSetKernelArg-A", status, true);
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_lineBuffer);
	checkStatus("clSetKernelArg-B", status, true);
  status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_computeKernelEdgeCount);
	checkStatus("clSetKernelArg-C", status, true);
  status = clSetKernelArg(kernel, 3, sizeof(cl_mem), &d_loc);
	checkStatus("clSetKernelArg-D", status, true);
	status = clSetKernelArg(kernel, 4, sizeof(int), &nRowsOfVertices);
	checkStatus("clSetKernelArg-E", status, true);
	status = clSetKernelArg(kernel, 5, sizeof(int), &nColsOfVertices);
	checkStatus("clSetKernelArg-F", status, true);
  status = clSetKernelArg(kernel, 6, sizeof(float), &level);
	checkStatus("clSetKernelArg-G", status, true);

	
	//-----------------------------------------------------
	// Configure the work-item structure
	//----------------------------------------------------- 

  // NO CHANGE

	//-----------------------------------------------------
	// Enqueue the kernel for execution
	//----------------------------------------------------- 

	status = clEnqueueNDRangeKernel(cmdQueue, kernel,
		numDimsToUse,
		nullptr, globalWorkSize, // globalOffset, globalSize
		localWorkSize,
		0, nullptr, nullptr); // event information, if needed
	checkStatus("clEnqueueNDRangeKernel", status, true);

	//-----------------------------------------------------
	// Read the output buffer back to the host
	//----------------------------------------------------- 
  	
  clEnqueueReadBuffer(cmdQueue, 
		d_computeKernelEdgeCount, CL_TRUE, 0, sizeof(int), 
		&numActualEdges, 0, nullptr, nullptr);

  std::cout << "NUMACTUALEDGES = " << numActualEdges << "\n";


  int numActualPoints = 2 * numActualEdges; // each edge is: (x,y), (x,y)
	// Get the point coords back, storing them into "lines"
  float* linearLineBuffer = new float[numActualEdges];
	lines = new vec2[numActualPoints];


	clEnqueueReadBuffer(cmdQueue, 
		d_lineBuffer, CL_TRUE, 0, numActualEdges*4*sizeof(float), 
		&linearLineBuffer, 0, nullptr, nullptr);

	// block until all commands have finished execution

	clFinish(cmdQueue);

  //////////////////////////////////////////////////////////////////////////
  //------------------ END COMPUTE KERNEL --------------------------------//
  //////////////////////////////////////////////////////////////////////////


  // releasing these causes a segfault for some reason? hmm 
  //clReleaseMemObject(d_vertexBuffer);
  //clReleaseMemObject(d_lineBuffer);
  //clReleaseMemObject(d_computeKernelEdgeCount);
	clReleaseProgram(program);
  clReleaseKernel(kernel);

	clReleaseCommandQueue(cmdQueue);
	clReleaseContext(context);
  
  









	
	// Use CUDA or OpenCL code to retrieve the points, placing them into "lines".
	// As a placeholder for now, we will just make an "X" over the area:
	lines[0][0] = 0.0; lines[0][1] = 0.0;
	lines[1][0] = nColsOfVertices - 1.0; lines[1][1] = nRowsOfVertices - 1.0;
	lines[2][0] = 0.0; lines[2][1] = nRowsOfVertices - 1.0;
	lines[3][0] = nColsOfVertices - 1.0; lines[3][1] = 0.0;

	// After the line end points have been returned from the device, delete the
	// device buffer to prevent a memory leak.
	// ... do it here ...
  
  delete [] platforms;
	delete [] devices;

  // Free host resources


	// return number of coordinate pairs in "lines":
	return 2;//numActualPoints;
}

void ContourGenerator::readData(std::ifstream& scalarFieldFile)
{
	vertexValues = new float[nRowsOfVertices * nColsOfVertices];
	int numRead = 0, numMissing = 0;
	float val;
	float minVal = 1.0, maxVal = -1.0;
	scalarFieldFile.read(reinterpret_cast<char*>(&val),sizeof(float));
	while (!scalarFieldFile.eof())
	{
		vertexValues[numRead++] = val;
		if (val == -9999)
			numMissing++;
		else if (minVal > maxVal)
			minVal = maxVal = val;
		else
		{
			if (val < minVal)
				minVal = val;
			else if (val > maxVal)
				maxVal = val;
		}
		scalarFieldFile.read(reinterpret_cast<char*>(&val),sizeof(float));
	}
	std::cout << "read " << numRead << " values; numMissing: " << numMissing
	          << "; range of values: " << minVal << " <= val <= " << maxVal << '\n';
}
