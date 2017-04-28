__kernel
void computeEdges( __global float* d_vertexBuffer, 
                 __global float* d_lineBuffer,
                 __global int* numEdges,
                 __global int* loc,
                  int rows,
                  int cols,
                  float value,)
                {

  int myCell = get_global_id(0);
  int myCol = myCell % cols;
  int myRow =(int)( myCell / cols);

  int numVertices = rows*cols;
  int numCells = (rows-1) * (cols-1);

  size_t gridSize = get_global_size(0);
  int numAbove = 0;

  for(int i = myCell; i < numVertices - cols; i += gridSize){
    //TODO: I THINK this is eliminating the top edge of vertices, but I'm not certain.
    if((myCell+1)%cols == 0){
      break;
    }

    float blVal = d_vertexBuffer[2*i];
    float brVal = d_vertexBuffer[2*i + 1];
    float tlVal = d_vertexBuffer[2*i + (cols*2)];
    float trVal = d_vertexBuffer[2*i + (cols*2) + 1];
    
    // Bottom left point differs
    if( ( blVal < value &&
          brVal > value &&
          tlVal > value &&
          trVal > value ) ||
        ( blVal > value &&
          brVal < value &&
          tlVal < value &&
          trVal < value )
        ){
      float x1 = 0.0;
      float y1 = 0.0;
      float x2 = 0.0;
      float y2 = 0.0;

      float f1 = (value - blVal) / (brVal - blVal);
      if(f1 < 0) { f1 = f1*(-1); }
      x1 = (1-f1)*myCol + f1*(myCol + 1);
      y1 = (1-f1)*myRow + f1*(myRow + 1);

      float f2 = (value - tlVal) / (tlVal = blVal);
      if(f2 < 0) { f2 = f2*(-1); }
      x2 = (1-f2)*myCol + f2*(myCol + 1);
      y2 = (1-f2)*myRow + f2*(myRow + 1);

      atomic_inc(d_numEdges);
      int k = atomic_add(&loc, 4);
      lineBuffer[k] = x1;
      lineBuffer[k+1] = y1;
      lineBuffer[k+2] = x2;
      lineBuffer[k+3] = y2;
    }
    




                   
  }


}
