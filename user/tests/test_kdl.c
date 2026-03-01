#include <stdio.h>
#include <unistd.h>

// Forward declaration of our syscall wrapper
void* load_library(const char* path);

int main(int argc, char** argv) 
{
    printf("Starting KDL dynamic loading test...\n");
    
    // Load the dynamic library
    void* lib_base = load_library("/initrd/math.kdl");
    
    if (lib_base == NULL || lib_base == (void*)0) 
    {
        printf("Failed to load /initrd/math.kdl\n");
        return 1;
    }
    
    printf("Successfully loaded math.kdl at 0x%lx\n", (unsigned long)lib_base);
    
    // In a real dlfcn implementation, we'd have dlsym() to parse the ELF 
    // export tables and find symbols by name.
    // For this proof-of-concept, we'll assume we know the offset of math_add
    // Alternatively, we can let objdump tell us the offset later, or 
    // we just verify the load succeeded for now and stop.
    // We will verify the load logic first.
    
    printf("Dynamic library test completed successfully.\n");
    return 0;
}
