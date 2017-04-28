
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

  int numVertices = rows*cols;

  int numAbove = 0;

  for(int i = myCell; i < numCells; i += gridSize){

    if(d_vertexBuffer[(myRow * cols * 2) + myCol*2] > value){
      numAbove++;
    }
    if(d_vertexBuffer[(myRow * cols * 2) + myCol*2 + 1] > value){
      numAbove++;
    }
    if(d_vertexBuffer[((myRow+1) * cols * 2) + myCol*2 + 1] > value){
      numAbove++;
    }
    if(d_vertexBuffer[((myRow+1) * cols * 2) + myCol*2 + 1] > value){
      numAbove++;
    }
    
    if( numAbove == 1 || numAbove == 3 ){
      
      atomic_inc(d_returnValue);  
    }
    if( numAbove == 2 ){ 
      atomic_inc(d_returnValue);  
      atomic_inc(d_returnValue);  
    }


                
  }


}
