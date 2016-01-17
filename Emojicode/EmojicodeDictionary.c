//
//  EmojicodeDictionary.c
//  Emojicode
//
//  Created by Theo Weidmann on 19/04/15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include "EmojicodeAPI.h"
#include "EmojicodeDictionary.h"
#include "EmojicodeString.h"

#define FNV_PRIME_64 1099511628211
#define FNV_OFFSET_64 14695981039346656037U
EmojicodeDictionaryHash fnv64(const char *k, size_t length) {
    EmojicodeDictionaryHash hash = FNV_OFFSET_64;
    for(size_t i = 0; i < length; i++){
        hash = hash ^ k[i];
        hash = hash * FNV_PRIME_64;
    }
    return hash;
}

EmojicodeDictionaryHash dictionaryHash(EmojicodeDictionary *dict, Something key) {
    // TODO: Implement hash Callable
    #define hashString(keyString) fnv64((char*)characters(keyString), ((keyString)->length) * sizeof(EmojicodeChar))
    return hashString((String *) key.object->value);
}

bool dictionaryKeyEqual(EmojicodeDictionary *dict, Something key1, Something key2) {
    return stringEqual((String *) key1.object->value, (String *) key2.object->value);
}

bool dictionaryKeyHashEqual(EmojicodeDictionary *dict, EmojicodeDictionaryHash hash1, EmojicodeDictionaryHash hash2, Something key1, Something key2) {
    return (hash1 == hash2) && dictionaryKeyEqual(dict, key1, key2);
}

// MARK: Internal dictionary
EmojicodeDictionaryNode* dictionaryGetNode(EmojicodeDictionary *dict, EmojicodeDictionaryHash hash, Something key) {
    EmojicodeDictionaryNode *e;
    Object** bucko;
    size_t n = 0;
    if (dict->buckets != NULL) {
        bucko = (Object**) dict->buckets->value;
        if ((n = dict->bucketsCounter) > 0) {
            Object *firsto = bucko[hash & (n - 1)];
            if (firsto != NULL) {
                e = firsto->value;
                if (dictionaryKeyHashEqual(dict, hash, e->hash, key, e->key)) {
                    return e;
                }
                Object *eo;
                while ((eo = e->next)) {
                    e = eo->value;
                    if (dictionaryKeyHashEqual(dict, hash, e->hash, key, e->key)) {
                        return e;
                    }
                }
            }
        }
    }
    return NULL;
}

Object* dictionaryNewNode(Object *dicto, EmojicodeDictionaryHash hash, Something key, Something value, Object *next, Thread *thread){
    stackPush(dicto, 0, 0, thread);
    Object *nodeo = newArray(sizeof(EmojicodeDictionaryNode));
    EmojicodeDictionaryNode *node = (EmojicodeDictionaryNode *) nodeo->value;
    stackPop(thread);
    
    node->hash = hash;
    node->key = key;
    node->value = value;
    node->next = next;
    return nodeo;
}

void dictionaryResize(Object *dicto, Thread *thread) {
    EmojicodeDictionary *dict = dicto->value;

    Object *oldBuckoo = dict->buckets;
    size_t oldCap = (oldBuckoo == NULL) ? 0 : dict->bucketsCounter;
    size_t oldThr = dict->nextThreshold;
    size_t newCap = oldCap << 1, newThr = 0;
    
    if (oldCap > 0) {
        if (oldCap >= DICTIONARY_MAXIMUM_CAPACTIY) {
            dict->nextThreshold = DICTIONARY_MAXIMUM_CAPACTIY_THRESHOLD;
            return;
        }
        else if (newCap < DICTIONARY_MAXIMUM_CAPACTIY && oldCap >= DICTIONARY_DEFAULT_INITIAL_CAPACITY) {
            newThr = oldThr << 1; // double threshold
        }
    }
    else if (oldThr > 0) { // initial capacity was placed in threshold
        newCap = oldThr;
    }
    else { // zero initial threshold signifies using defaults
        newCap = DICTIONARY_DEFAULT_INITIAL_CAPACITY;
        newThr = (size_t) (DICTIONARY_DEFAULT_LOAD_FACTOR * DICTIONARY_DEFAULT_INITIAL_CAPACITY);
    }
    
    if (newThr == 0) {
        float ft = (float) newCap * dict->loadFactor;
        newThr = (newCap < DICTIONARY_MAXIMUM_CAPACTIY && ft < (float)DICTIONARY_MAXIMUM_CAPACTIY) ? (size_t)ft : DICTIONARY_MAXIMUM_CAPACTIY_THRESHOLD;
    }
    
    stackPush(dicto, 0, 0, thread);
    Object *newBuckoo = newArray(newCap * sizeof(Object *));
    dict = stackGetThis(thread)->value;
    stackPop(thread);
    
    dict->buckets = newBuckoo;
    dict->nextThreshold = newThr;
    dict->bucketsCounter = newCap;
    
    Object **newBucko = newBuckoo->value;
    if (oldBuckoo != NULL) {
        for (int j = 0; j < oldCap; ++j) {
            Object **oldBucko = oldBuckoo->value;
            Object *eo = oldBucko[j];
            if (eo != NULL) {
                EmojicodeDictionaryNode *e = eo->value;
                oldBucko[j] = NULL;
                if (e->next == NULL) {
                    newBucko[e->hash & (newCap - 1)] = eo;
                }
                else { // preserve order
                    Object *loHeado = NULL, *loTailo = NULL;
                    Object *hiHeado = NULL, *hiTailo = NULL;
                    Object *nexto;
                    do {
                        e = eo->value;
                        nexto = e->next;
                        if ((e->hash & oldCap) == 0) {
                            if (loTailo == NULL) {
                                loHeado = eo;
                            }
                            else {
                                EmojicodeDictionaryNode *loTail = loTailo->value;
                                loTail->next = eo;
                            }
                            loTailo = eo;
                        }
                        else {
                            if (hiTailo == NULL) {
                                hiHeado = eo;
                            }
                            else {
                                EmojicodeDictionaryNode *hiTail = hiTailo->value;
                                hiTail->next = eo;
                            }
                            hiTailo = eo;
                        }
                    } while ((eo = nexto) != NULL);
                    
                    if (loTailo != NULL) {
                        EmojicodeDictionaryNode *loTail = loTailo->value;
                        loTail->next = NULL;
                        newBucko[j] = loHeado;
                    }
                    if(hiTailo != NULL) {
                        EmojicodeDictionaryNode *hiTail = hiTailo->value;
                        hiTail->next = NULL;
                        newBucko[j + oldCap] = hiHeado;
                    }
                }
            }
        }
    }
}

void dictionaryPutVal(Object *dicto, Something key, Something value, Thread *thread) {
    EmojicodeDictionaryHash hash = dictionaryHash(dicto->value, key);
    
    EmojicodeDictionary *dict = dicto->value;
    
    if (dict->buckets == NULL || dict->bucketsCounter == 0) {
        dictionaryResize(dicto, thread);
    }
    
    Object **bucko;
    size_t n = 0, i = 0;
    bucko = dict->buckets->value;
    n = dict->bucketsCounter;
    
    Object *po;
    
    if ((po = bucko[i = (hash & (n - 1))]) == NULL) {
        bucko[i] = dictionaryNewNode(dicto, hash, key, value, NULL, thread);
    }
    else {
        EmojicodeDictionaryNode *p = po->value;
        Object *eo = NULL;
        if (dictionaryKeyHashEqual(dict, hash, p->hash, key, p->key)) {
            eo = po;
        }
        else {
            for (int binCount = 0; ; ++binCount) {
                if (p->next == NULL) {
                    p->next = dictionaryNewNode(dicto, hash, key, value, NULL, thread);
                    dict = dicto->value;
                    break;
                }
                EmojicodeDictionaryNode *e = p->next->value;
                
                if (dictionaryKeyHashEqual(dict, hash, e->hash, key, e->key)) {
                    break;
                }
                p = e;
            }
        }
        if (eo != NULL) { // existing mapping for key
            EmojicodeDictionaryNode *e = eo->value;
            e->value = value;
            return;
        }
    }
    if(++(dict->size) > dict->nextThreshold) {
        dictionaryResize(dicto, thread);
    }
}

EmojicodeDictionaryNode* dictionaryRemoveNode(EmojicodeDictionary *dict, EmojicodeDictionaryHash hash, Something key, Thread *thread){
    size_t n = 0, index = 0;
    if (dict->buckets != NULL && (n = dict->bucketsCounter) > 0) {
        Object **bucko = dict->buckets->value;
        Object *po = bucko[index = hash & (n - 1)];
        if (po != NULL) {
            EmojicodeDictionaryNode *p = po->value;
            EmojicodeDictionaryNode *node = NULL;
            if (dictionaryKeyHashEqual(dict, hash, p->hash, key, p->key)) {
                node = p;
            }
            else {
                Object *nexto = p->next;
                while (nexto) {
                    EmojicodeDictionaryNode *e = nexto->value;
                    if (dictionaryKeyHashEqual(dict, hash, e->hash, key, e->key)) {
                        node = e;
                        break;
                    }
                    p = e;
                    nexto = e->next;
                }
            }
            if(node != NULL) {
                if (node == p) {
                    bucko[index] = node->next;
                }
                else {
                    p->next = node->next;
                }
                dict->size--;
                return node;
            }
        }
    }
    return NULL;
}


// MARK: Bridge -> Dictionary interface
void dictionaryRemove(EmojicodeDictionary *dict, Something key, Thread *thread) {
    dictionaryRemoveNode(dict, dictionaryHash(dict, key), key, thread);
}

bool dictionaryContainsKey(EmojicodeDictionary *dict, Something key) {
    return dictionaryGetNode(dict, dictionaryHash(dict, key), key) != NULL;
}

void dictionaryClear(EmojicodeDictionary *dict) {
    if (dict->buckets != NULL && dict->size > 0) {
        EmojicodeDictionaryNode **buck = dict->buckets->value;
        dict->size = 0;
        for (int i = 0; i < dict->bucketsCounter; ++i) {
            buck[i] = NULL;
        }
    }
}

void dictionaryInit(Thread *thread) {
    EmojicodeDictionary *dict = stackGetThis(thread)->value;
    dict->loadFactor = DICTIONARY_DEFAULT_LOAD_FACTOR;
    dict->size = 0;
    dict->buckets = NULL;
    dict->nextThreshold = 0;
}

void dictionaryMark(Object *object) {
    EmojicodeDictionary *dict = object->value;
    
    if(dict->buckets == NULL){
        return;
    }
    mark(&dict->buckets);
    
    Object **buckets = dict->buckets->value;
    for (size_t i = 0; i < dict->bucketsCounter; i++) {
        Object **eo = &buckets[i];
        while (*(eo)) {
            mark(eo);
            EmojicodeDictionaryNode *e = (*eo)->value;
            if(isRealObject(e->key)){
                mark(&(e->key.object));
            }
            if(isRealObject(e->value)){
                mark(&(e->value.object));
            }
            eo = &(e->next);
        }
    }
}

void dictionarySet(Object *dicto, Something key, Something value, Thread *thread){
    dictionaryPutVal(dicto, key, value, thread);
}

//MARK: Bridges

static Something bridgeDictionarySet(Thread *thread) {
    dictionarySet(stackGetThis(thread), stackGetVariable(0, thread), stackGetVariable(1, thread), thread);
    return NOTHINGNESS;
}

static Something bridgeDictionaryGet(Thread *thread) {
    Something key = stackGetVariable(0, thread);
    EmojicodeDictionaryNode *node = dictionaryGetNode(stackGetThis(thread)->value, dictionaryHash(stackGetThis(thread)->value, key), key);
    if(node == NULL){
        return NOTHINGNESS;
    }
    else {
        return node->value;
    }
}

static Something bridgeDictionaryRemove(Thread *thread) {
    dictionaryRemove(stackGetThis(thread)->value, stackGetVariable(0, thread), thread);
    return NOTHINGNESS;
}

void bridgeDictionaryInit(Thread *thread) {
    dictionaryInit(thread);
}

MethodHandler dictionaryMethodForName(EmojicodeChar name) {
    switch (name) {
        case 0x1F43D: //🐽
            return bridgeDictionaryGet;
        case 0x1F428: //🐨
            return bridgeDictionaryRemove;
        case 0x1F437: //🐷
            return bridgeDictionarySet;
    }
    return NULL;
}
