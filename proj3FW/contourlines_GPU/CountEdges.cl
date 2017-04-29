
__kernel
void countEdges( __global float* d_vertexBuffer, 
                 __global int* d_returnValue,
                  int rows,
                  int cols,
                  float value)
                {

  int myCell = get_global_id(0);
  int numCells = (rows-1) * (cols-1);
 
  size_t gridSize = get_global_size(0);

  for(int i = myCell; i < numCells; i += gridSize){
    // Address of bottom left vertex of cell
    int myCol = i % (cols-1);
    int myRow =(int)( i / (cols-1));


    float blVal = d_vertexBuffer[myRow * cols + myCol];
    float brVal = d_vertexBuffer[myRow * cols + myCol + 1];
    float tlVal = d_vertexBuffer[(myRow+1) * cols  + myCol];
    float trVal = d_vertexBuffer[(myRow+1) * cols  + myCol + 1];

    if(blVal != -9999 && brVal != -9999 && tlVal != -9999 && trVal != -9999){
    //if(blVal == -9999 || brVal == -9999 || tlVal == -9999 || trVal == -9999){
    
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
}
