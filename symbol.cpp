/******************************************************************************
 *  CVS version:
 *     $Id: symbol.c,v 1.3 2004/05/05 22:00:08 nickie Exp $
 ******************************************************************************
 *
 *  C code file : symbol.c
 *  Project     : PCL Compiler
 *  Version     : 1.0 alpha
 *  Written by  : Nikolaos S. Papaspyrou (nickie@softlab.ntua.gr)
 *  Date        : May 14, 2003
 *  Description : Generic symbol table in C
 *
 *  Comments: (in Greek iso-8859-7)
 *  ---------
 *  ������ �������� �����������.
 *  ����� ������������ ��������� ��� ��������� �����������.
 *  ������ ����������� ������������ ��� �����������.
 *  ���������� ����������� ����������
 */


/* ---------------------------------------------------------------------
   ---------------------------- Header files ---------------------------
   --------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "general.h"
#include "error.h"
#include "symbol.h"


/* ---------------------------------------------------------------------
   ------------- ��������� ���������� ��� ������ �������� --------------
   --------------------------------------------------------------------- */

Scope        * currentScope;           /* �������� ��������              */
unsigned int   quadNext;               /* ������� �������� ��������      */
unsigned int   tempNumber;             /* �������� ��� temporaries       */

static unsigned int   hashTableSize;   /* ������� ������ ��������������� */
static SymbolEntry ** hashTable;       /* ������� ���������������        */

static struct Type_tag typeConst [] = {
    { TYPE_VOID,    NULL, 0, 0 },
    { TYPE_INTEGER, NULL, 0, 0 },
    { TYPE_BOOLEAN, NULL, 0, 0 },
    { TYPE_CHAR,    NULL, 0, 0 },
    { TYPE_REAL,    NULL, 0, 0 }, 
};

const Type typeVoid    = &(typeConst[0]);
const Type typeInteger = &(typeConst[1]);
const Type typeBoolean = &(typeConst[2]);
const Type typeChar    = &(typeConst[3]);
const Type typeReal    = &(typeConst[4]);
const Type typeString  = typeIArray(typeChar);


/* ---------------------------------------------------------------------
   ------- ��������� ���������� ����������� ��� ������ �������� --------
   --------------------------------------------------------------------- */

typedef unsigned long int HashType;

static HashType PJW_hash (const char * key)
{
    /*
     *  P.J. Weinberger's hashing function. See also:
     *  Aho A.V., Sethi R. & Ullman J.D, "Compilers: Principles,
     *  Techniques and Tools", Addison Wesley, 1986, pp. 433-437.
     */

    const HashType PJW_OVERFLOW =
        (((HashType) 0xf) << (8 * sizeof(HashType) - 4));
    const int PJW_SHIFT = (8 * (sizeof(HashType) - 1));
    
    HashType h, g;
    
    for (h = 0; *key != '\0'; key++) {
        h = (h << 4) + (*key);
        if ((g = h & PJW_OVERFLOW) != 0) {
            h ^= g >> PJW_SHIFT;
            h ^= g;
        }
    }
    return h;
}

void strAppendChar (char * buffer, RepChar c)
{
    switch (c) {
        case '\n':
            strcat(buffer, "\\n");
            break;
        case '\t':
            strcat(buffer, "\\t");
            break;
        case '\r':
            strcat(buffer, "\\r");
            break;
        case '\0':
            strcat(buffer, "\\0");
            break;
        case '\\':
            strcat(buffer, "\\\\");
            break;
        case '\'':
            strcat(buffer, "\\'");
            break;
        case '\"':
            strcat(buffer, "\\\"");
            break;
        default: {
            char s[] = { '\0', '\0' };
            
            *s = c;
            strcat(buffer, s);
        }
    }
}

void strAppendString (char * buffer, RepString str)
{
    const char * s;
    
    for (s = str; *s != '\0'; s++)
        strAppendChar(buffer, *s);
}


/* ---------------------------------------------------------------------
   ------ ��������� ��� ����������� ��������� ��� ������ �������� ------
   --------------------------------------------------------------------- */

void initSymbolTable (unsigned int size)
{
    unsigned int i;
    
    /* �������� �������������� */
    
    currentScope = NULL;
    quadNext     = 1;
    tempNumber   = 1;
    
    /* ������������ ��� ������ ��������������� */
    
    hashTableSize = size;
    hashTable = (SymbolEntry **) newAlloc(size * sizeof(SymbolEntry *));
    
    for (i = 0; i < size; i++)
        hashTable[i] = NULL;
}

void destroySymbolTable ()
{
    unsigned int i;
    
    /* ���������� ��� ������ ��������������� */
    
    for (i = 0; i < hashTableSize; i++)
        if (hashTable[i] != NULL)
            destroyEntry(hashTable[i]);

    deleteAlloc(hashTable);
}

void openScope ()
{
    Scope * newScope = (Scope *) newAlloc(sizeof(Scope));

    newScope->negOffset = START_NEGATIVE_OFFSET;
    newScope->parent    = currentScope;
    newScope->entries   = NULL;

    if (currentScope == NULL)
        newScope->nestingLevel = 0;
    else
        newScope->nestingLevel = currentScope->nestingLevel + 1;
    
    currentScope = newScope;
}

/* function to open new scope inside a block */
/* idea by Leonidas Tsepenekas */ 
void blockScope (unsigned int start_offset)
{
    Scope * newScope = (Scope *) newAlloc(sizeof(Scope));

    newScope->negOffset    = start_offset;
    newScope->parent       = currentScope;
    newScope->entries      = NULL;
    newScope->nestingLevel = 2;

    //if (currentScope->nestingLevel == 1)
    //    newScope->negOffset -= currentScope->negOffset;

    currentScope = newScope;
}

void closeScope_original ()
{
    SymbolEntry * e = currentScope->entries;
    Scope       * t = currentScope;
    
    while (e != NULL) {
        SymbolEntry * next = e->nextInScope;
        
        hashTable[e->hashValue] = e->nextHash;
        destroyEntry(e);
        e = next;
    }
    
    currentScope = currentScope->parent;
    deleteAlloc(t);
}

void closeScope ()
{
    //printSymbolTable();
    if (currentScope->nestingLevel >= 2)
        currentScope->parent->negOffset = currentScope->negOffset;

    SymbolEntry * e = currentScope->entries;
    Scope       * t = currentScope;
    
    while (e != NULL) {
        SymbolEntry * next = e->nextInScope;
        
        hashTable[e->hashValue] = e->nextHash;
        destroyEntry(e);
        e = next;
    }
    
    currentScope = currentScope->parent;
    deleteAlloc(t);
}

static void insertEntry (SymbolEntry * e)
{
    e->nextHash             = hashTable[e->hashValue];
    hashTable[e->hashValue] = e;
    e->nextInScope          = currentScope->entries;
    currentScope->entries   = e;
}

static SymbolEntry * newEntry (const char * name)
{
    SymbolEntry * e;
    
    /* ������� �� ������� ��� */
    
    for (e = currentScope->entries; e != NULL; e = e->nextInScope)
        if (strcmp(name, e->id) == 0) {
            error("Duplicate identifier: %s", name);
            return NULL;
        }

    /* ������������ ���� �����: entryType ��� u */

    e = (SymbolEntry *) newAlloc(sizeof(SymbolEntry));
    e->id = (const char *) newAlloc(strlen(name) + 1);

    strcpy((char *) (e->id), name);
    e->hashValue    = PJW_hash(name) % hashTableSize;
    e->nestingLevel = currentScope->nestingLevel;
    e->final_code_name = NULL;
    e->isGlobal = false;
    insertEntry(e);
    return e;
}

SymbolEntry * newVariable (const char * name, Type type)
{
    SymbolEntry * e = newEntry(name);
    
    if (e != NULL) {
        e->entryType = ENTRY_VARIABLE;
        e->u.eVariable.type = type;
        type->refCount++;
        currentScope->negOffset -= sizeOfType(type);
        e->u.eVariable.offset = currentScope->negOffset;
    }
    return e;
}

SymbolEntry * newConstant (const char * name, Type type, ...)
{
    SymbolEntry * e;
    va_list ap;

    union {
        RepInteger vInteger;
        RepBoolean vBoolean;
        RepChar    vChar;
        RepReal    vReal;
        RepString  vString;
    } value;
         
    va_start(ap, type);
    switch (type->kind) {
        case TYPE_INTEGER:
            value.vInteger = va_arg(ap, RepInteger);
            break;
        case TYPE_BOOLEAN:
            value.vBoolean = va_arg(ap, int);     /* RepBool is promoted */
            break;
        case TYPE_CHAR:
            value.vChar = va_arg(ap, int);        /* RepChar is promoted */
            break;
        case TYPE_REAL:
            value.vReal = va_arg(ap, RepReal);
            break;
        case TYPE_ARRAY:
            if (equalType(type->refType, typeChar)) {
                RepString str = va_arg(ap, RepString);
                
                value.vString = (const char *) newAlloc(strlen(str) + 1);
                strcpy((char *) (value.vString), str);
                break;
            }
        default:
            internal("Invalid type for constant");
    }
    va_end(ap);

    if (name == NULL) {
        char buffer[256];
        
        switch (type->kind) {
            case TYPE_INTEGER:
                sprintf(buffer, "%d", value.vInteger);
                break;
            case TYPE_BOOLEAN:
                if (value.vBoolean)
                    sprintf(buffer, "true");
                else
                    sprintf(buffer, "false");
                break;
            case TYPE_CHAR:
                strcpy(buffer, "'");
                strAppendChar(buffer, value.vChar);
                strcat(buffer, "'");
                break;
            case TYPE_REAL:
                sprintf(buffer, "%Lg", value.vReal);
                break;
            case TYPE_ARRAY:
                strcpy(buffer, "\"");
                strAppendString(buffer, value.vString);
                strcat(buffer, "\"");      
                break;
            default:
                break;
        }
        e = newEntry(buffer);
    }
    else
        e = newEntry(name);
    
    if (e != NULL) {
        e->entryType = ENTRY_CONSTANT;
        e->u.eConstant.type = type;
        type->refCount++;
        switch (type->kind) {
            case TYPE_INTEGER:
                e->u.eConstant.value.vInteger = value.vInteger;
                break;
            case TYPE_BOOLEAN:
                e->u.eConstant.value.vBoolean = value.vBoolean;
                break;
            case TYPE_CHAR:
                e->u.eConstant.value.vChar = value.vChar;
                break;
            case TYPE_REAL:
                e->u.eConstant.value.vReal = value.vReal;
                break;
            case TYPE_ARRAY:
                e->u.eConstant.value.vString = value.vString;
                break;
            default:
                break;
        }
    }
    return e;
}

SymbolEntry * newFunction (const char * name)
{
    SymbolEntry * e = lookupEntry(name, LOOKUP_CURRENT_SCOPE, false);

    if (e == NULL) {
        e = newEntry(name);
        if (e != NULL) {
            e->entryType = ENTRY_FUNCTION;
            e->u.eFunction.isForward = false;
            e->u.eFunction.pardef = PARDEF_DEFINE;
            e->u.eFunction.firstArgument = e->u.eFunction.lastArgument = NULL;
            e->u.eFunction.resultType = NULL;
        }
        return e;
    }
    else if (e->entryType == ENTRY_FUNCTION && e->u.eFunction.isForward) {
        e->u.eFunction.isForward = false;
        e->u.eFunction.pardef = PARDEF_CHECK;
        e->u.eFunction.lastArgument = NULL;
        return e;
    }
    else {
       error("Duplicate identifier: %s", name);
       return NULL;
    }
}

SymbolEntry * newParameter (const char * name, Type type,
                            PassMode mode, SymbolEntry * f)
{
    SymbolEntry * e;
    
    if (f->entryType != ENTRY_FUNCTION)
        internal("Cannot add a parameter to a non-function");
    switch (f->u.eFunction.pardef) {
        case PARDEF_DEFINE:
            e = newEntry(name);
            if (e != NULL) {
                e->entryType = ENTRY_PARAMETER;
                e->u.eParameter.type = type;
                type->refCount++;
                e->u.eParameter.mode = mode;
                e->u.eParameter.next = NULL;
            }
            if (f->u.eFunction.lastArgument == NULL)
                f->u.eFunction.firstArgument = f->u.eFunction.lastArgument = e;
            else {
                f->u.eFunction.lastArgument->u.eParameter.next = e;
                f->u.eFunction.lastArgument = e;
            }
            return e;            
        case PARDEF_CHECK:
            e = f->u.eFunction.lastArgument;
            if (e == NULL)
                e = f->u.eFunction.firstArgument;
            else
                e = e->u.eParameter.next;
            if (e == NULL)
                error("More parameters than expected in redeclaration "
                      "of function %s", f->id);
            else if (!equalType(e->u.eParameter.type, type))
                error("Parameter type mismatch in redeclaration "
                      "of function %s", f->id);
            else if (e->u.eParameter.mode != mode)
                error("Parameter passing mode mismatch in redeclaration "
                      "of function %s", f->id);
            else if (strcmp(e->id, name) != 0)
                error("Parameter name mismatch in redeclaration "
                      "of function %s", f->id);
            else
                insertEntry(e);
            f->u.eFunction.lastArgument = e;
            return e;
        case PARDEF_COMPLETE:
            fatal("Cannot add a parameter to an already defined function");
    }
    return NULL;
}

static unsigned int fixOffset (SymbolEntry * args, int res_addr_space)
{
    if (args == NULL)
        return 0;
    else {
        unsigned int rest = fixOffset(args->u.eParameter.next, res_addr_space);
        
        args->u.eParameter.offset = START_POSITIVE_OFFSET + res_addr_space + rest;
        if (args->u.eParameter.mode == PASS_BY_REFERENCE)
            return rest + 4;
        else
            return rest + max(sizeOfType(args->u.eParameter.type), 4);
    }
}

void forwardFunction (SymbolEntry * f)
{
    if (f->entryType != ENTRY_FUNCTION)
        internal("Cannot make a non-function forward");
    f->u.eFunction.isForward = true;
}

void endFunctionHeader (SymbolEntry * f, Type type)
{
    if (f->entryType != ENTRY_FUNCTION)
        internal("Cannot end parameters in a non-function");
    switch (f->u.eFunction.pardef) {
        case PARDEF_COMPLETE:
            internal("Cannot end parameters in an already defined function");
            break;
        case PARDEF_DEFINE:
            if (type->kind != TYPE_VOID) // if it returns a value
                fixOffset(f->u.eFunction.firstArgument, 4);   // return address space = 4
            else
                fixOffset(f->u.eFunction.firstArgument, 0);   // else no space is needed
            f->u.eFunction.resultType = type;
            type->refCount++;
            break;
        case PARDEF_CHECK:
            if ((f->u.eFunction.lastArgument != NULL &&
                 f->u.eFunction.lastArgument->u.eParameter.next != NULL) ||
                (f->u.eFunction.lastArgument == NULL &&
                 f->u.eFunction.firstArgument != NULL))
                error("Fewer parameters than expected in redeclaration "
                      "of function %s", f->id);
            if (!equalType(f->u.eFunction.resultType, type))
                error("Result type mismatch in redeclaration of function %s",
                      f->id);
            break;
    }
    f->u.eFunction.pardef = PARDEF_COMPLETE;
}

SymbolEntry * newTemporary(Type type)
{
    char * buffer = (char *)newAlloc(10);
    SymbolEntry * e;

    sprintf(buffer, "$%d", tempNumber);
    e = newEntry(buffer);
     
    if (e != NULL) {
        e->entryType = ENTRY_TEMPORARY;
        e->u.eVariable.type = type;
        type->refCount++;
        currentScope->negOffset -= sizeOfType(type);
        e->u.eTemporary.offset = currentScope->negOffset;
        e->u.eTemporary.number = tempNumber++;
        e->u.eTemporary.name = buffer;
        e->u.eTemporary.isConst = false;
        e->u.eTemporary.isReference = false;
        e->u.eTemporary.isArrayElement = false;
    }
    return e;
}

// creates a temporary for the result of an array quad
// the result is a the address of the ith element of the array
SymbolEntry * newTemporaryRef(Type type)
{
    char * buffer = (char *)newAlloc(10);
    SymbolEntry * e;

    sprintf(buffer, "$%d", tempNumber);
    e = newEntry(buffer);
     
    if (e != NULL) {
        e->entryType = ENTRY_TEMPORARY;
        e->u.eVariable.type = type;
        type->refCount++;
        currentScope->negOffset -= 4; // size of a pointer
        e->u.eTemporary.offset = currentScope->negOffset;
        e->u.eTemporary.number = tempNumber++;
        e->u.eTemporary.name = buffer;
        e->u.eTemporary.isConst = false;
        e->u.eTemporary.isReference = true;
        e->u.eTemporary.isArrayElement = false;
    }
    return e;
}


SymbolEntry * newTemporary(Type type, VALUE *value)
{
    char *buffer = (char *)newAlloc(10);
    SymbolEntry * e;

    sprintf(buffer, "$%d", tempNumber);
    e = newEntry(buffer);
    
    if (e != NULL) {
        e->entryType = ENTRY_TEMPORARY;
        e->u.eVariable.type = type;
        type->refCount++;
        //currentScope->negOffset -= sizeOfType(type);
        e->u.eTemporary.offset = currentScope->negOffset;
        e->u.eTemporary.number = tempNumber++;
        e->u.eTemporary.isConst = true;
        e->u.eTemporary.isReference = false;
        e->u.eTemporary.name = buffer;
        e->u.eTemporary.value = *value;
    }
    return e;
}

void destroyEntry (SymbolEntry * e)
{
    SymbolEntry * args;
    
    switch (e->entryType) {
        case ENTRY_VARIABLE:
            destroyType(e->u.eVariable.type);
            break;
        case ENTRY_CONSTANT:
            if (e->u.eConstant.type->kind == TYPE_ARRAY)
                deleteAlloc((char *) (e->u.eConstant.value.vString));
            destroyType(e->u.eConstant.type);
            break;
        case ENTRY_FUNCTION:
            args = e->u.eFunction.firstArgument;
            while (args != NULL) {
                SymbolEntry * p = args;
                
                destroyType(args->u.eParameter.type);
                deleteAlloc((char *) (args->id));
                args = args->u.eParameter.next;
                deleteAlloc(p);
            }
            destroyType(e->u.eFunction.resultType);
            break;
        case ENTRY_PARAMETER:
            /* �� ���������� �������������� ���� �� �� ��������� */
            return;
        case ENTRY_TEMPORARY:
            destroyType(e->u.eTemporary.type);
            break;
    }
    deleteAlloc((char *) (e->id));
    deleteAlloc(e);        
}

SymbolEntry * lookupEntry (const char * name, LookupType type, bool err)
{
    unsigned int  hashValue = PJW_hash(name) % hashTableSize;
    SymbolEntry * e         = hashTable[hashValue];
    
    switch (type) {
        case LOOKUP_CURRENT_SCOPE:
            while (e != NULL && e->nestingLevel == currentScope->nestingLevel)
                if (strcmp(e->id, name) == 0)
                    return e;
                else
                    e = e->nextHash;
            break;
        case LOOKUP_ALL_SCOPES:
            while (e != NULL)
                if (strcmp(e->id, name) == 0)
                    return e;
                else
                    e = e->nextHash;
            break;
    }
    
    if (err)
        error("Unknown identifier: %s", name);
    return NULL;
}

Type typeArray (RepInteger size, Type refType)
{
    Type n = (Type) newAlloc(sizeof(struct Type_tag));

    n->kind     = TYPE_ARRAY;
    n->refType  = refType;
    n->size     = size;
    n->refCount = 1;
    
    refType->refCount++;

    return n;
}

Type typeIArray (Type refType)
{
    Type n = (Type) newAlloc(sizeof(struct Type_tag));

    n->kind     = TYPE_IARRAY;
    n->refType  = refType;
    n->refCount = 1;
    
    refType->refCount++;

    return n;
}

Type typePointer (Type refType)
{
    Type n = (Type) newAlloc(sizeof(struct Type_tag));

    n->kind     = TYPE_POINTER;
    n->refType  = refType;
    n->refCount = 1;
    
    refType->refCount++;

    return n;
}

void destroyType (Type type)
{
    switch (type->kind) {
        case TYPE_ARRAY:
        case TYPE_IARRAY:
        case TYPE_POINTER:
            if (--(type->refCount) == 0) {
                destroyType(type->refType);
                deleteAlloc(type);
            }
            break;
        default:
            break;
    }
}

unsigned int sizeOfType (Type type)
{
    switch (type->kind) {
        case TYPE_VOID:
            internal("Type void has no size");
            break;
        case TYPE_INTEGER:
        case TYPE_IARRAY:
        case TYPE_POINTER:
            return 4;
        case TYPE_BOOLEAN:
        case TYPE_CHAR:
            return 1;
        case TYPE_REAL:
            return 10;
        case TYPE_ARRAY:
            return type->size * sizeOfType(type->refType);
        default:
            break;
    }
    return 0;
}

bool equalType (Type type1, Type type2)
{
    if (type1->kind != type2->kind)
        return false;
    switch (type1->kind) {
        case TYPE_ARRAY:
            if (type1->size != type2->size)
                return false;
        case TYPE_IARRAY:
        case TYPE_POINTER:
            return equalType(type1->refType, type2->refType);
        default:
            break;
    }
    return true;        
}

void printType (Type type)
{
    if (type == NULL) {
        printf("<undefined>");
        return;
    }
    
    switch (type->kind) {
        case TYPE_VOID:
            printf("void");
            break;
        case TYPE_INTEGER:
            printf("integer");
            break;
        case TYPE_BOOLEAN:
            printf("boolean");
            break;
        case TYPE_CHAR:
            printf("char");
            break;
        case TYPE_REAL:
            printf("real");
            break;
        case TYPE_ARRAY:
            printf("array [%d] of ", type->size);
            printType(type->refType);
            break;
        case TYPE_IARRAY:
            printf("array of ");
            printType(type->refType);
            break;
        case TYPE_POINTER:
            printf("^");
            printType(type->refType);
            break;
    }
}

void printMode (PassMode mode)
{
    if (mode == PASS_BY_REFERENCE)
        printf("var ");
}


#define SHOW_OFFSETS

void printSymbolTable ()
{
    Scope       * scp;
    SymbolEntry * e;
    SymbolEntry * args;
    
    scp = currentScope;
    if (scp == NULL)
        printf("no scope\n");
    else
        while (scp != NULL) {
            printf("scope: ");
            e = scp->entries;
            while (e != NULL) {
                if (e->entryType == ENTRY_TEMPORARY)
                    printf("$%d", e->u.eTemporary.number);
                else
                    printf("%s", e->id);
                switch (e->entryType) {
                    case ENTRY_FUNCTION:
                        printf("(");
                        args = e->u.eFunction.firstArgument;
                        while (args != NULL) {
                            printMode(args->u.eParameter.mode);
                            printf("%s : ", args->id);
                            printType(args->u.eParameter.type);
                            args = args->u.eParameter.next;
                            if (args != NULL)
                                printf("; ");
                        }
                        printf(") : ");
                        printType(e->u.eFunction.resultType);
                        break;
#ifdef SHOW_OFFSETS
                    case ENTRY_VARIABLE:
                        printf("[%d]", e->u.eVariable.offset);
                        break;
                    case ENTRY_PARAMETER:
                        printf("[%d]", e->u.eParameter.offset);
                        break;
                    case ENTRY_TEMPORARY:
                        printf("[%d]", e->u.eTemporary.offset);
                        break;
#endif
                    default:
                        break;
                }
                e = e->nextInScope;
                if (e != NULL)
                    printf(", ");
            }
            scp = scp->parent;
            printf("\n");
        }
    printf("Current negOffset = %d\n", currentScope->negOffset);
    printf("Current nesting level = %d\n", currentScope->nestingLevel);
    printf("----------------------------------------\n");
}

//number of parameters of a func/proc

int num_param(const char *id)
{
    int n=1;
    SymbolEntry *e   = lookupEntry(id, LOOKUP_ALL_SCOPES, false);
    if (e->entryType != ENTRY_FUNCTION) return -1;
    SymbolEntry *h = e->u.eFunction.firstArgument;
    SymbolEntry *t = e->u.eFunction.lastArgument;
    while (h!=t && h) {
        n++;
        h = h->u.eParameter.next;
    }
    return n;
}


