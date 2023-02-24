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

void cacheSimulator(FILE *file, struct Cache *cache, int associativity, int blockSize, int numSets, char *replacementPolicy){
    int age = 0;
    int memReads = 0;
    int memWrites = 0;
    int cacheHits = 0;
    int cacheMisses = 0;
    char memAction;
    unsigned long address;
    unsigned long index;
    unsigned long tag;
    bool cacheHit;
    bool spaceAvailable;
    while(fscanf(file, "%c %lx %*[\n]", &memAction, &address) != EOF){
        cacheHit = false;
        spaceAvailable = false;
        index = computeIndex(address, numSets, blockSize);
        tag = computeTag(address, numSets, blockSize);
        for(int i = 0; i < associativity; i ++){
            //Check for valid blocks with same tag
            if(cache->setArray[index]->blockArray[i]->tag == tag && cache->setArray[index]->blockArray[i]->valid == 1){
                cacheHit = true;
                cacheHits ++;
                //update age if lru
                if(strcmp(replacementPolicy, "lru") == 0){
                    cache->setArray[index]->blockArray[i]->age = age;
                }
                //update memwrites if applicable
                if(memAction == 'W'){
                    memWrites ++;
                }
                break;
            }
        }
        //if cache miss
        if(!cacheHit){
            cacheMisses ++;
            for(int i = 0; i < associativity; i ++){
                //check for first invalid block to put new block in
                if(cache->setArray[index]->blockArray[i]->valid == 0){
                    spaceAvailable = true;
                    cache->setArray[index]->blockArray[i]->valid = 1;
                    cache->setArray[index]->blockArray[i]->tag = tag;
                    cache->setArray[index]->blockArray[i]->age = age;
                    break;
                }
            }
            //if all blocks are valid then it is a collision
            if(!spaceAvailable){
                int tempIndex = 0;
                int tempAge = cache->setArray[index]->blockArray[0]->age;
                for(int i = 1; i < associativity; i ++){
                    //find index of block with lowest age
                    if(cache->setArray[index]->blockArray[i]->age < cache->setArray[index]->blockArray[i-1]->age){
                        if(cache->setArray[index]->blockArray[i]->age < tempAge){
                            tempAge = cache->setArray[index]->blockArray[i]->age;
                            tempIndex = i;
                        }
                    }
                    else{
                        if(cache->setArray[index]->blockArray[i - 1]->age < tempAge){
                            tempAge = cache->setArray[index]->blockArray[i - 1]->age;
                            tempIndex = i - 1;
                        }
                    }
                }
                //replace the block with the lowest age and update age
                cache->setArray[index]->blockArray[tempIndex]->tag = tag;
                cache->setArray[index]->blockArray[tempIndex]->age = age;
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
    printf("memread:%d\nmemwrite:%d\ncachehit:%d\ncachemiss:%d\n", memReads, memWrites, cacheHits, cacheMisses);
    fclose(file);
}

int main(int argc, char* argv[argc + 1]){
    int cacheSize = atoi(argv[1]);
    char* associativityArg = argv[2];
    char associativityString[strlen(associativityArg)];
    strncpy(associativityString, &associativityArg[6], strlen(associativityArg));
    int associativity = atoi(associativityString);
    char* replacementPolicy = argv[3];
    int blockSize = atoi(argv[4]);
    int numSets = cacheSize / associativity / blockSize;
    FILE *file = fopen(argv[5], "r");
    if(file == NULL){
        printf("error");
    }
    else{
        //printf("cache size:%d assoc:%d policy:%s block size:%d numSets:%d\n", cacheSize, associativity, replacementPolicy, blockSize, numSets);
        struct Cache *cache = newCache(numSets, associativity, blockSize);
        cacheSimulator(file, cache, associativity, blockSize, numSets, replacementPolicy);
        freeCache(cache);
    }
    return EXIT_SUCCESS;
}