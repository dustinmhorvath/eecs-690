__kernel
void countEdges( __global float* d_vertexBuffer, 
                 __global int* d_returnValue,
                  int rows,
                  int cols,
                  float value)
                {

  int myCell = get_global_id(0);

  int numVertices = rows*cols;

  size_t gridSize = get_global_size(0);
  int numAbove = 0;
  int numBelow = 0;

  // < (numvertices-1)*2 because I don't want to catch the edges
  for(int i = myCell*2; i < (numVertices)*2; i += gridSize){
    //TODO: I THINK this is eliminating the top edge of vertices, but I'm not certain.
    //if(myCell%cols == 1){
    //  break;
    //}

    if(d_vertexBuffer[i] > value){
      numAbove++;
    }
    else numBelow++;
    if(d_vertexBuffer[i+1] > value){
      numAbove++;
    }
    else numBelow++;
    if(d_vertexBuffer[i+(cols*2)] > value){
      numAbove++;
    }
    else numBelow++;
    if(d_vertexBuffer[i+(cols*2)+1] > value){
      numAbove++;
    }
    else numBelow++;



    if(   (numAbove == 1 && numBelow == 3) ||
          (numAbove == 3 && numBelow == 1)  ){
      
      atomic_inc(d_returnValue);  
    }
    if(   (numAbove == 2 && numBelow == 2) ){ 
      atomic_inc(d_returnValue);  
      atomic_inc(d_returnValue);  
    }


                
  }


}
