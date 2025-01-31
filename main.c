#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// here we implement arraylist ////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    int* arr;
    size_t used;
    size_t size;
} ArrayList;

void createArrayList(ArrayList* dArr, size_t initialSize) {
    dArr->arr = (int*)malloc(initialSize * sizeof(int));
    dArr->used = 0;
    dArr->size = initialSize;
}

void insert(ArrayList* dArr, int element) {
    // if no space left, expand
    if (dArr->used == dArr->size) {
        dArr->size *= 2;
        int* temp = (int*)realloc(dArr->arr, dArr->size * sizeof(int));
        if (!temp) {
            printf("Error: realloc failed for ArrayList.\n");
            exit(1);
        }
        dArr->arr = temp;
    }
    dArr->arr[dArr->used++] = element;
}

int get(ArrayList* dArr, int index) {
    if (index < 0 || index >= (int)dArr->used) {
        printf("Index out of bounds in ArrayList.\n");
        exit(1);
    }
    return dArr->arr[index];
}

void freeArrayList(ArrayList* dArr) {
    free(dArr->arr);
    dArr->arr = NULL;
    dArr->used = 0;
    dArr->size = 0;
}
////////////////////////////////////////////////////////////////////////////////////////

// here we define node types ////////////////////////////////////////////////////////////////////////////////////////
typedef enum {
    AND,
    OR,
    NOT,
    XOR,
    INPUT,
    OUTPUT
} Type;

// here we implement the node struct, now with memoization fields ////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    Type type;
    uint32_t uniqueID;
    ArrayList inputs;   // IDs of input nodes
    ArrayList outputs;  // IDs of output nodes

    // For INPUT nodes, 'value' is the bit assigned in main().
    int value;          

    // Memoization fields
    int hasCachedValue; // 0 = not computed yet for this combination, 1 = cachedValue is valid
    int cachedValue;    
} Node;

// createNode initializes defaults, including memo fields
Node* createNode(void) {
    Node* this = (Node*)malloc(sizeof(Node));
    if (this == NULL) {
        printf("Error: malloc failed!\n");
        return NULL;
    }
    this->type = INPUT;  // default
    this->uniqueID = 0;
    createArrayList(&(this->inputs), 16);
    createArrayList(&(this->outputs), 16);
    this->value = 0;
    this->hasCachedValue = 0;
    this->cachedValue = 0;
    return this;
}
////////////////////////////////////////////////////////////////////////////////////////

// here we have an arraylist of node pointers ////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    Node** arr;
    size_t used;
    size_t size;
} ArrayListNode;

void createArrayListNode(ArrayListNode* dArr, size_t initialSize) {
    dArr->arr = (Node**)malloc(initialSize * sizeof(Node*));
    dArr->used = 0;
    dArr->size = initialSize;
}

void insertNode(ArrayListNode* dArr, Node* nodePtr) {
    if (dArr->used == dArr->size) {
        dArr->size *= 2;
        Node** temp = (Node**)realloc(dArr->arr, dArr->size * sizeof(Node*));
        if (!temp) {
            printf("Error: realloc failed for ArrayListNode.\n");
            exit(1);
        }
        dArr->arr = temp;
    }
    dArr->arr[dArr->used++] = nodePtr;
}

Node* getNode(ArrayListNode* dArr, int index) {
    if (index < 0 || index >= (int)dArr->used) {
        printf("Index out of bounds in ArrayListNode.\n");
        exit(1);
    }
    return dArr->arr[index];
}

void freeArrayListNode(ArrayListNode* dArr) {
    free(dArr->arr);
    dArr->arr = NULL;
    dArr->used = 0;
    dArr->size = 0;
}

void freeAllNodes(ArrayListNode* nodes) {
    for (int i = 0; i < (int)nodes->used; i++) {
        Node* n = nodes->arr[i];
        freeArrayList(&n->inputs);
        freeArrayList(&n->outputs);
        free(n);
    }
    freeArrayListNode(nodes);
}
////////////////////////////////////////////////////////////////////////////////////////

// here we parse the input file  ////////////////////////////////////////////////////////////////////////////////////////
char* trimWhitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++; 
    if (*str == 0) return str;                  

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; 
    end[1] = '\0';
    return str;
}

Type getTypeFromString(const char* typeStr) {
    if (strcmp(typeStr, "AND") == 0)    return AND;
    if (strcmp(typeStr, "OR") == 0)     return OR;
    if (strcmp(typeStr, "NOT") == 0)    return NOT;
    if (strcmp(typeStr, "XOR") == 0)    return XOR;
    if (strcmp(typeStr, "INPUT") == 0)  return INPUT;
    if (strcmp(typeStr, "OUTPUT") == 0) return OUTPUT;
    printf("Error: Unknown gate type '%s'\n", typeStr);
    exit(1);
}

void parseConnections(const char* line, ArrayList* list, const char* label) {
    const char* start = line + strlen(label);

    char* lineCopy = strdup(start);
    if (!lineCopy) {
        printf("Error: strdup failed.\n");
        exit(1);
    }

    char* token = strtok(lineCopy, ",");
    while (token) {
        token = trimWhitespace(token);
        if (*token != '\0') {
            insert(list, atoi(token));
        }
        token = strtok(NULL, ",");
    }
    free(lineCopy);
}

void parseFile(const char* filename, ArrayListNode* nodes) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file: %s\n", filename);
        exit(1);
    }

    char line[256];
    Node* currentNode = NULL;
    int insideOuterBlock = 0;
    int insideStanza = 0;

    while (fgets(line, sizeof(line), file)) {
        char* trimmedLine = trimWhitespace(line);
        if (*trimmedLine == '\0' || *trimmedLine == '#') {
            continue; // skip empty or comment lines
        }

        if (strcmp(trimmedLine, "{") == 0) {
            if (!insideOuterBlock) {
                insideOuterBlock = 1; // top-level block
            } else if (!insideStanza) {
                insideStanza = 1; // start of node stanza
                currentNode = createNode();
            }
            continue;
        }

        if (strcmp(trimmedLine, "}") == 0) {
            if (insideStanza) {
                insertNode(nodes, currentNode);
                currentNode = NULL;
                insideStanza = 0;
            } else if (insideOuterBlock) {
                insideOuterBlock = 0;
            }
            continue;
        }

        if (!insideStanza) {
            printf("Error: Unexpected content outside of a stanza: %s\n", trimmedLine);
            exit(1);
        }

        // Parse properties of the node inside the stanza
        if (strstr(trimmedLine, "Type=")) {
            char typeStr[32];
            sscanf(trimmedLine, "Type=%31s", typeStr);
            currentNode->type = getTypeFromString(typeStr);
        }
        else if (strstr(trimmedLine, "UniqueID=")) {
            int id;
            if (sscanf(trimmedLine, "UniqueID=%d", &id) != 1 || id < 0) {
                printf("Error: Invalid or missing UniqueID on line: %s\n", trimmedLine);
                exit(1);
            }
            currentNode->uniqueID = id;
        }
        else if (strstr(trimmedLine, "Input=")) {
            parseConnections(trimmedLine, &(currentNode->inputs), "Input=");
        }
        else if (strstr(trimmedLine, "Output=")) {
            parseConnections(trimmedLine, &(currentNode->outputs), "Output=");
        }
        else {
            printf("Error: Unrecognized line within stanza: %s\n", trimmedLine);
            exit(1);
        }
    }

    fclose(file);

    if (insideOuterBlock || insideStanza) {
        printf("Error: Unclosed block or stanza detected.\n");
        exit(1);
    }
}
////////////////////////////////////////////////////////////////////////////////////////

// here we implement the dfs with memoization  ////////////////////////////////////////////////////////////////////////////////////////

// AND operation
int bitwiseAND(ArrayList* in) {
    if (in->used < 2) {
        printf("Error: Not enough inputs for AND.\n");
        exit(1);
    }
    int output = get(in, 0);
    for (int i = 1; i < (int)in->used; i++) {
        output &= get(in, i);
    }
    return output;
}

// OR operation
int bitwiseOR(ArrayList* in) {
    if (in->used < 2) {
        printf("Error: Not enough inputs for OR.\n");
        exit(1);
    }
    int output = get(in, 0);
    for (int i = 1; i < (int)in->used; i++) {
        output |= get(in, i);
    }
    return output;
}

// NOT operation
int bitwiseNOT(ArrayList* in) {
    if (in->used != 1) {
        printf("Error: NOT requires exactly one input.\n");
        exit(1);
    }
    // Only the LSB matters for typical boolean logic, so we mask with &1:
    return ~get(in, 0) & 1;
}

// XOR operation
int bitwiseXOR(ArrayList* in) {
    if (in->used < 2) {
        printf("Error: Not enough inputs for XOR.\n");
        exit(1);
    }
    int output = get(in, 0);
    for (int i = 1; i < (int)in->used; i++) {
        output ^= get(in, i);
    }
    return output;
}

// find the corresponding node based on its unique ID (linear search)
Node* findNodeByID(ArrayListNode* nodes, int ID) {
    for (int i = 0; i < (int)nodes->used; i++) {
        if (nodes->arr[i]->uniqueID == (uint32_t)ID) {
            return nodes->arr[i];
        }
    }
    printf("Error: Node with UniqueID=%d not found.\n", ID);
    exit(1);
}

// DFS with memoization
int dfs(ArrayListNode* nodes, Node* currentNode) {
    if (!currentNode) {
        printf("Error: Null node encountered in DFS.\n");
        exit(1);
    }

    // 1) If we already computed this node's output in this combination, return it.
    if (currentNode->hasCachedValue) {
        return currentNode->cachedValue;
    }

    // 2) If it's an INPUT node, its "value" is directly assigned
    if (currentNode->type == INPUT) {
        currentNode->cachedValue = currentNode->value;
        currentNode->hasCachedValue = 1;
        return currentNode->cachedValue;
    }

    // 3) If it's an OUTPUT node, pass through the first input
    if (currentNode->type == OUTPUT) {
        if (currentNode->inputs.used < 1) {
            printf("Error: OUTPUT node has no inputs.\n");
            exit(1);
        }
        int inID = get(&currentNode->inputs, 0);
        Node* inNode = findNodeByID(nodes, inID);

        currentNode->cachedValue = dfs(nodes, inNode);
        currentNode->hasCachedValue = 1;
        return currentNode->cachedValue;
    }

    // 4) For AND, OR, NOT, XOR => gather input values
    ArrayList inputValues;
    createArrayList(&inputValues, 10);

    for (int i = 0; i < (int)currentNode->inputs.used; i++) {
        int inID = get(&currentNode->inputs, i);
        Node* inNode = findNodeByID(nodes, inID);
        int inVal = dfs(nodes, inNode);
        insert(&inputValues, inVal);
    }

    // 5) Compute output based on gate type
    int outputValue = 0;
    switch (currentNode->type) {
        case AND: 
            outputValue = bitwiseAND(&inputValues); 
            break;
        case OR:  
            outputValue = bitwiseOR(&inputValues);  
            break;
        case NOT: 
            outputValue = bitwiseNOT(&inputValues); 
            break;
        case XOR: 
            outputValue = bitwiseXOR(&inputValues); 
            break;
        default:
            printf("Error: Unsupported gate type in DFS.\n");
            exit(1);
    }
    freeArrayList(&inputValues);

    // 6) Cache the computed value and return it
    currentNode->cachedValue = outputValue;
    currentNode->hasCachedValue = 1;
    return outputValue;
}
////////////////////////////////////////////////////////////////////////////////////////

// comparator for qsort by unique ID
static int compareNodePtrByID(const void* a, const void* b) {
    Node* n1 = *(Node**)a;
    Node* n2 = *(Node**)b;
    return (int)(n1->uniqueID - n2->uniqueID);
}
////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("A file was not provided to analyze.\n");
        return 1;
    }

    const char* fileName = argv[1];

    // 1) Parse the file for nodes
    ArrayListNode nodes;
    createArrayListNode(&nodes, 10);
    parseFile(fileName, &nodes);

    // 2) Separate the input and output nodes into separate lists
    ArrayListNode inputNodes;
    createArrayListNode(&inputNodes, 10);
    ArrayListNode outputNodes;
    createArrayListNode(&outputNodes, 10);

    for (int i = 0; i < (int)nodes.used; i++) {
        Node* n = getNode(&nodes, i);
        if (n->type == INPUT) {
            insertNode(&inputNodes, n);
        }
        else if (n->type == OUTPUT) {
            insertNode(&outputNodes, n);
        }
    }

    // 3) Sort the input and output nodes by ID
    qsort(inputNodes.arr, inputNodes.used, sizeof(Node*), compareNodePtrByID);
    qsort(outputNodes.arr, outputNodes.used, sizeof(Node*), compareNodePtrByID);

    // 4) Prepare for truth table
    int numInputs = (int)inputNodes.used;
    int numOutputs = (int)outputNodes.used;
    int numCombinations = (int)pow(2, numInputs);

    // 5) Print the header row (Input IDs, then Output IDs)
    for (int i = 0; i < numInputs; i++) {
        Node* inNode = inputNodes.arr[i];
        printf("%d ", inNode->uniqueID);
    }
    printf("| ");
    for (int i = 0; i < numOutputs; i++) {
        Node* outNode = outputNodes.arr[i];
        if (i < numOutputs - 1) {
            printf("%d ", outNode->uniqueID);
        } else {
            printf("%d", outNode->uniqueID);
        }
    }
    printf("\n");

    // 6) For each combination, assign bits to inputs, reset memo flags, compute outputs
    for (int combination = 0; combination < numCombinations; combination++) {
        // Set INPUT node bits (lowest bit assigned to inputNodes.arr[0], etc.)
        for (int i = 0; i < numInputs; i++) {
            int bitVal = (combination >> i) & 1; 
            inputNodes.arr[i]->value = bitVal;
        }

        // Reset memoization on all nodes for this combination
        for (int i = 0; i < (int)nodes.used; i++) {
            nodes.arr[i]->hasCachedValue = 0;
        }

        // Print input values in ascending ID order (matching the order we sorted them)
        for (int i = 0; i < numInputs; i++) {
            printf("%d ", inputNodes.arr[i]->value);
        }
        printf("| ");

        // Evaluate and print each output in ascending ID order
        for (int i = 0; i < numOutputs; i++) {
            int result = dfs(&nodes, outputNodes.arr[i]);
            if (i < numOutputs - 1) {
                printf("%d ", result);
            } else {
               printf("%d", result); 
            }
        }

        if (combination < numCombinations - 1) {
            printf("\n");
        }
    }

    // 7) Free all memory
    freeArrayListNode(&inputNodes);
    freeArrayListNode(&outputNodes);
    freeAllNodes(&nodes);

    return 0;
}
