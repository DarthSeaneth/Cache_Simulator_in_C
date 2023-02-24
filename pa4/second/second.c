#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

struct Cache{
    int numSets;
    struct Set **setArray;
};

struct Set{
    unsigned long index;
    int numBlocks;
    struct Block **blockArray;
};

struct Block{
    int valid;
    unsigned long tag;
    int blockSize;
    int age;
    unsigned long address;
};

struct Cache *newCache(int numSets, int associativity, int blockSize){
    unsigned long index = 0;
    struct Cache *cache = malloc(sizeof(struct Cache));
    cache->numSets = numSets;
    cache->setArray = malloc(numSets * sizeof(struct Set));
    for(int i = 0; i < numSets; i ++){
        cache->setArray[i] = malloc(sizeof(struct Set));
        cache->setArray[i]->index = index;
        cache->setArray[i]->numBlocks = associativity;
        cache->setArray[i]->blockArray = malloc(associativity * sizeof(struct Block));
        index ++;
        for(int j = 0; j < associativity; j ++){
            cache->setArray[i]->blockArray[j] = malloc(sizeof(struct Block));
            cache->setArray[i]->blockArray[j]->valid = 0;
            cache->setArray[i]->blockArray[j]->tag = 0;
            cache->setArray[i]->blockArray[j]->blockSize = blockSize;
            cache->setArray[i]->blockArray[j]->age = -1;
            cache->setArray[i]->blockArray[j]->address = 0;
        }
    }
    return cache;
}

void freeCache(struct Cache *cache){
    for(int i = 0; i < cache->numSets; i ++){
        for(int j = 0; j < cache->setArray[i]->numBlocks; j ++){
            free(cache->setArray[i]->blockArray[j]);
        }
        free(cache->setArray[i]->blockArray);
        free(cache->setArray[i]);
    }
    free(cache->setArray);
    free(cache);
}

unsigned long computeIndex(unsigned long address, int numSets, int blockSize){
    unsigned long b = log2l(blockSize);
    return ((address >> b) & (numSets - 1));
}

unsigned long computeTag(unsigned long address, int numSets, int blockSize){
    unsigned long b = log2l(blockSize);
    unsigned long n = log2l(numSets);
    return (address >> (b + n));
}

void cacheSimulator(FILE *file, struct Cache *L1Cache, struct Cache *L2Cache, int L1Associativity, int L2Associativity, int blockSize, int L1NumSets, int L2NumSets, char *L1Policy, char *L2Policy){
    int age = 0;
    int memReads = 0;
    int memWrites = 0;
    int L1CacheHits = 0;
    int L1CacheMisses = 0;
    int L2CacheHits = 0;
    int L2CacheMisses = 0;
    char memAction;
    unsigned long address;
    unsigned long index;
    unsigned long tag;
    bool L1CacheHit;
    bool L2CacheHit;
    bool L1SpaceAvailable;
    bool L2SpaceAvailable;
    while(fscanf(file, "%c %lx %*[\n]", &memAction, &address) != EOF){
        L1CacheHit = false;
        L2CacheHit = false;
        L1SpaceAvailable = false;
        L2SpaceAvailable = false;
        index = computeIndex(address, L1NumSets, blockSize);
        tag = computeTag(address, L1NumSets, blockSize);
        for(int i = 0; i < L1Associativity; i ++){
            //Check for valid blocks with same tag
            if(L1Cache->setArray[index]->blockArray[i]->tag == tag && L1Cache->setArray[index]->blockArray[i]->valid == 1){
                L1CacheHit = true;
                L1CacheHits ++;
                L1Cache->setArray[index]->blockArray[i]->address = address;
                //update age if lru
                if(strcmp(L1Policy, "lru") == 0){
                    L1Cache->setArray[index]->blockArray[i]->age = age;
                }
                //update memwrites if applicable
                if(memAction == 'W'){
                    memWrites ++;
                }
                break;
            }
        }
        //if L1 cache miss, check L2
        if(!L1CacheHit){
            L1CacheMisses ++;
            index = computeIndex(address, L2NumSets, blockSize);
            tag = computeTag(address, L2NumSets, blockSize);
            for(int i = 0; i < L2Associativity; i ++){
                //if L2 cache hit, evict and move to L1
                if(L2Cache->setArray[index]->blockArray[i]->tag == tag && L2Cache->setArray[index]->blockArray[i]->valid == 1){
                    L2CacheHit = true;
                    L2CacheHits ++;
                    //invalidate block in L2 before moving to L1
                    L2Cache->setArray[index]->blockArray[i]->valid = 0;
                    break;
                }
            }
        }
        if(L2CacheHit){ //put evicted block in L1, and move any evicted block from L1 to L2
            index = computeIndex(address, L1NumSets, blockSize);
            tag = computeTag(address, L1NumSets, blockSize);
            for(int i = 0; i < L1Associativity; i ++){
                //check for first invalid block to put evicted block in
                if(L1Cache->setArray[index]->blockArray[i]->valid == 0){
                    L1SpaceAvailable = true;
                    L1Cache->setArray[index]->blockArray[i]->valid = 1;
                    L1Cache->setArray[index]->blockArray[i]->tag = tag;
                    L1Cache->setArray[index]->blockArray[i]->age = age;
                    L1Cache->setArray[index]->blockArray[i]->address = address;
                    break;
                }
            }
            //if all blocks are valid then it is a collision, need to evict block to make space, and move evicted block to L2
            if(!L1SpaceAvailable){
                int tempIndex = 0;
                int tempAge = L1Cache->setArray[index]->blockArray[0]->age;
                for(int i = 1; i < L1Associativity; i ++){
                    //find index of block with lowest age
                    if(L1Cache->setArray[index]->blockArray[i]->age < L1Cache->setArray[index]->blockArray[i-1]->age){
                        if(L1Cache->setArray[index]->blockArray[i]->age < tempAge){
                            tempAge = L1Cache->setArray[index]->blockArray[i]->age;
                            tempIndex = i;
                        }
                    }
                    else{
                        if(L1Cache->setArray[index]->blockArray[i - 1]->age < tempAge){
                            tempAge = L1Cache->setArray[index]->blockArray[i - 1]->age;
                            tempIndex = i - 1;
                        }
                    }
                }
                //replace the block with the lowest age and update age
                unsigned long tempAddress = L1Cache->setArray[index]->blockArray[tempIndex]->address;
                L1Cache->setArray[index]->blockArray[tempIndex]->tag = tag;
                L1Cache->setArray[index]->blockArray[tempIndex]->age = age;
                L1Cache->setArray[index]->blockArray[tempIndex]->address = address;
                //move evicted block to L2
                index = computeIndex(tempAddress, L2NumSets, blockSize);
                tag = computeTag(tempAddress, L2NumSets, blockSize);
                for(int i = 0; i < L2Associativity; i ++){
                    if(L2Cache->setArray[index]->blockArray[i]->valid == 0){
                        L2SpaceAvailable = true;
                        L2Cache->setArray[index]->blockArray[i]->valid = 1;
                        L2Cache->setArray[index]->blockArray[i]->tag = tag;
                        L2Cache->setArray[index]->blockArray[i]->age = age;
                        L2Cache->setArray[index]->blockArray[i]->address = tempAddress;
                        break;
                    }
                }
                if(!L2SpaceAvailable){
                    tempIndex = 0;
                    tempAge = L2Cache->setArray[index]->blockArray[0]->age;
                    for(int i = 1; i < L2Associativity; i ++){
                        //find index of block with lowest age
                        if(L2Cache->setArray[index]->blockArray[i]->age < L2Cache->setArray[index]->blockArray[i-1]->age){
                            if(L2Cache->setArray[index]->blockArray[i]->age < tempAge){
                                tempAge = L2Cache->setArray[index]->blockArray[i]->age;
                                tempIndex = i;
                            }
                        }
                        else{
                            if(L2Cache->setArray[index]->blockArray[i - 1]->age < tempAge){
                                tempAge = L2Cache->setArray[index]->blockArray[i - 1]->age;
                                tempIndex = i - 1;
                            }
                        }
                    }
                    //replace the block with the lowest age and update age
                    L2Cache->setArray[index]->blockArray[tempIndex]->tag = tag;
                    L2Cache->setArray[index]->blockArray[tempIndex]->age = age;
                    L2Cache->setArray[index]->blockArray[tempIndex]->address = tempAddress;
                }
            }
            //update memwrites if applicable
            if(memAction == 'W'){
                memWrites ++;
            }
        }
        //if L1 & L2 cache miss
        if(!L2CacheHit && !L1CacheHit){
            L2CacheMisses ++;
            L1SpaceAvailable = false;
            L2SpaceAvailable = false;
            index = computeIndex(address, L1NumSets, blockSize);
            tag = computeTag(address, L1NumSets, blockSize);
            for(int i = 0; i < L1Associativity; i ++){
                //check for first invalid block to put new block in
                if(L1Cache->setArray[index]->blockArray[i]->valid == 0){
                    L1SpaceAvailable = true;
                    L1Cache->setArray[index]->blockArray[i]->valid = 1;
                    L1Cache->setArray[index]->blockArray[i]->tag = tag;
                    L1Cache->setArray[index]->blockArray[i]->age = age;
                    L1Cache->setArray[index]->blockArray[i]->address = address;
                    break;
                }
            }
            //if all blocks are valid then it is a collision
            if(!L1SpaceAvailable){
                int tempIndex = 0;
                int tempAge = L1Cache->setArray[index]->blockArray[0]->age;
                for(int i = 1; i < L1Associativity; i ++){
                    //find index of block with lowest age
                    if(L1Cache->setArray[index]->blockArray[i]->age < L1Cache->setArray[index]->blockArray[i-1]->age){
                        if(L1Cache->setArray[index]->blockArray[i]->age < tempAge){
                            tempAge = L1Cache->setArray[index]->blockArray[i]->age;
                            tempIndex = i;
                        }
                    }
                    else{
                        if(L1Cache->setArray[index]->blockArray[i - 1]->age < tempAge){
                            tempAge = L1Cache->setArray[index]->blockArray[i - 1]->age;
                            tempIndex = i - 1;
                        }
                    }
                }
                //replace the block with the lowest age and update age
                unsigned long tempAddress = L1Cache->setArray[index]->blockArray[tempIndex]->address;
                L1Cache->setArray[index]->blockArray[tempIndex]->tag = tag;
                L1Cache->setArray[index]->blockArray[tempIndex]->age = age;
                L1Cache->setArray[index]->blockArray[tempIndex]->address = address;
                //move evicted block to L2
                index = computeIndex(tempAddress, L2NumSets, blockSize);
                tag = computeTag(tempAddress, L2NumSets, blockSize);
                for(int i = 0; i < L2Associativity; i ++){
                    if(L2Cache->setArray[index]->blockArray[i]->valid == 0){
                        L2SpaceAvailable = true;
                        L2Cache->setArray[index]->blockArray[i]->valid = 1;
                        L2Cache->setArray[index]->blockArray[i]->tag = tag;
                        L2Cache->setArray[index]->blockArray[i]->age = age;
                        L2Cache->setArray[index]->blockArray[i]->address = tempAddress;
                        break;
                    }
                }
                if(!L2SpaceAvailable){
                    tempIndex = 0;
                    tempAge = L2Cache->setArray[index]->blockArray[0]->age;
                    for(int i = 1; i < L2Associativity; i ++){
                        //find index of block with lowest age
                        if(L2Cache->setArray[index]->blockArray[i]->age < L2Cache->setArray[index]->blockArray[i-1]->age){
                            if(L2Cache->setArray[index]->blockArray[i]->age < tempAge){
                                tempAge = L2Cache->setArray[index]->blockArray[i]->age;
                                tempIndex = i;
                            }
                        }
                        else{
                            if(L2Cache->setArray[index]->blockArray[i - 1]->age < tempAge){
                                tempAge = L2Cache->setArray[index]->blockArray[i - 1]->age;
                                tempIndex = i - 1;
                            }
                        }
                    }
                    //replace the block with the lowest age and update age
                    L2Cache->setArray[index]->blockArray[tempIndex]->tag = tag;
                    L2Cache->setArray[index]->blockArray[tempIndex]->age = age;
                    L2Cache->setArray[index]->blockArray[tempIndex]->address = tempAddress;
                }
            }
            if(memAction == 'R'){
                memReads ++;
            }
            else{
                memReads ++;
                memWrites ++;
            }
        }
        age ++;
    }
    printf("memread:%d\nmemwrite:%d\nl1cachehit:%d\nl1cachemiss:%d\nl2cachehit:%d\nl2cachemiss:%d\n", memReads, memWrites, L1CacheHits, L1CacheMisses, L2CacheHits, L2CacheMisses);
    fclose(file);
}

int main(int argc, char* argv[argc + 1]){
    int L1CacheSize = atoi(argv[1]);
    char* L1AssociativityArg = argv[2];
    char L1AssociativityString[strlen(L1AssociativityArg)];
    strncpy(L1AssociativityString, &L1AssociativityArg[6], strlen(L1AssociativityArg));
    int L1Associativity = atoi(L1AssociativityString);
    char* L1Policy = argv[3];
    int blockSize = atoi(argv[4]);
    int L1NumSets = L1CacheSize / L1Associativity / blockSize;
    int L2CacheSize = atoi(argv[5]);
    char* L2AssociativityArg = argv[6];
    char L2AssociativityString[strlen(L2AssociativityArg)];
    strncpy(L2AssociativityString, &L2AssociativityArg[6], strlen(L2AssociativityArg));
    int L2Associativity = atoi(L2AssociativityString);
    char* L2Policy = argv[7];
    int L2NumSets = L2CacheSize / L2Associativity / blockSize;
    FILE *file = fopen(argv[8], "r");
    if(file == NULL){
        printf("error");
    }
    else{
        struct Cache *L1Cache = newCache(L1NumSets, L1Associativity, blockSize);
        struct Cache *L2Cache = newCache(L2NumSets, L2Associativity, blockSize);
        cacheSimulator(file, L1Cache, L2Cache, L1Associativity, L2Associativity, blockSize, L1NumSets, L2NumSets, L1Policy, L2Policy);
        freeCache(L1Cache);
        freeCache(L2Cache);
    }
    return EXIT_SUCCESS;
}