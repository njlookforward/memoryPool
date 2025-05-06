#ifndef MEMORYPOOL_COMMON_H
#define MEMORYPOOL_COMMON_H

// 相信手中的代码，只有敲下去才是真实的，每次回来之后，哪怕抄也可以抄一抄代码，真的挺舒服的，这个思考过程
// 一点点推敲

namespace memoryPool {

#define ALIGNS 8
#define MAX_SIZE 256 * 1024
#define FREE_LIST_SIZE MAX_SIZE/ALIGNS

}


#endif