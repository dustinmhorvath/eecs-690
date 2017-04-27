__kernel
void countEdges( __global float* d_vertexBuffer, 
                  __global int* d_returnValue,
                  int rows,
                  int cols,
                  float value)
                {


  int colID = get_global_id(0);
  int rowID = get_global_id(1);
  int myStartPoint = (rowID*cols + colID) * 2;

  int numVertices = rows*cols;

  size_t globalSize = get_global_size(0);
  int numAbove = 0;
  int numBelow = 0;

  for(int i = myStartPoint; i < numVertices*2; i += globalSize){
    for(int j = 0; j < 4; j++){
      if(d_vertexBuffer[i+j] > value){
        numAbove++;
      }
      if(d_vertexBuffer[i+j] < value){
        numBelow++;
      }
    }

    if(   (numAbove == 1 && numBelow == 3) ||
          (numAbove == 3 && numBelow == 1)  ){
      
      atomic_inc(d_returnValue);  
    }
    if(   (numAbove == 2 && numBelow == 2) ){ 
      atomic_inc(d_returnValue);  
      atomic_inc(d_returnValue);  
    }

    
    numAbove = 0;
    numBelow = 0;


                
  }



}
