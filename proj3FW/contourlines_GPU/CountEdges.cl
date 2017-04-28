
__kernel
void countEdges( __global float* d_vertexBuffer, 
                 __global int* d_returnValue,
                  int rows,
                  int cols,
                  float value)
                {

  int myCell = get_global_id(0);
  int numCells = (rows-1) * (cols-1);
 
  // Address of bottom left vertex of cell
  int myCol = myCell % (cols-1);
  int myRow =(int)( myCell / (cols-1));

  size_t gridSize = get_global_size(0);

  for(int i = myCell; i < numCells; i += gridSize){

    float blVal = d_vertexBuffer[myRow * cols * 2 + myCol*2];
    float brVal = d_vertexBuffer[myRow * cols * 2 + myCol*2 + 1];
    float tlVal = d_vertexBuffer[(myRow+1) * cols * 2 + myCol*2];
    float trVal = d_vertexBuffer[(myRow+1) * cols * 2 + myCol*2 + 1];
    
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
      atomic_inc(d_returnValue);
    }
    // Bottom Right point differs
    if( ( blVal > value &&
          brVal < value &&
          tlVal > value &&
          trVal > value ) ||
        ( blVal < value &&
          brVal > value &&
          tlVal < value &&
          trVal < value )
        ){
      atomic_inc(d_returnValue);
    }
    // Top left point differs
    if( ( blVal > value &&
          brVal > value &&
          tlVal < value &&
          trVal > value ) ||
        ( blVal < value &&
          brVal < value &&
          tlVal > value &&
          trVal < value )
        ){
      atomic_inc(d_returnValue);
    }
    // Top Right point differs
    if( ( blVal > value &&
          brVal > value &&
          tlVal > value &&
          trVal < value ) ||
        ( blVal < value &&
          brVal < value &&
          tlVal < value &&
          trVal > value )
        ){
      atomic_inc(d_returnValue);
    }
    // Left and Right differ
    if( ( blVal < value &&
          brVal > value &&
          tlVal < value &&
          trVal > value ) ||
        ( blVal > value &&
          brVal < value &&
          tlVal > value &&
          trVal < value )
        ){
      atomic_inc(d_returnValue);
    }
    // Top and Bottom differ
    if( ( blVal > value &&
          brVal > value &&
          tlVal < value &&
          trVal < value ) ||
        ( blVal < value &&
          brVal < value &&
          tlVal > value &&
          trVal > value )
        ){
      atomic_inc(d_returnValue);
    }
    // Caddy corner
    if( ( blVal > value &&
          brVal < value &&
          tlVal < value &&
          trVal > value ) ||
        ( blVal < value &&
          brVal > value &&
          tlVal > value &&
          trVal < value )
        ){
      atomic_add(d_returnValue, 2);
    }
  }
}
