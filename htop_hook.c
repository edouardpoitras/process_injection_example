#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <subhook.h>

// Global variable holding our subhook type.
subhook_t hook;

/**
  * Helper funtion to retrieve a symbol in the current process by name.
  * 
  * This will be used to find the address location of the various
  * subhook functions we need to perform the hook.
  */
void* get_symbol_by_name(const char* name) {
  // Get a handle to this library by using NULL.
  void* handle = dlopen(NULL, RTLD_LAZY);
  if (!handle) {
    printf("Failed to dlopen hook dynamic library: %s\n", dlerror());
    exit(1);
  }
  // Get the symbol we want by name.
  void* sym = dlsym(handle, name); 
  if (!sym) {
    printf("Failed to dlsym %s: %s\n", name, dlerror());
    exit(1);
  }
  // Close our handle and clear any errors.
  dlclose(handle);
  dlerror();
  return sym;
}

/**
  * Our hook - will replace the existing Platform_getUptime() function in the htop process.
  * The function definitions must match the original Platform_getUptime (other than the name).
  *
  * If we wanted to temporarily disable the hook in order to call the original
  * Platform_getUptime() function we could do something like this:
  *
  *   subhook_remove(hook);
  *   int uptime = Platform_getUptime();
  *   // Do stuff with return value...
  *   subhook_install(hook);
  *   return uptime;
  */
int Platform_getUptime_Hook() {
  // Our hook will simply make it so the Uptime value in htop always says 0.
  return 0;
}

/**
  * The attribute is to ensure this function will execute upon injection.
  * This is crucial or else our hook would never be installed.
  *
  * A lot of the code here is simply to load up the subhook function we need.
  * I'm not sure this is the best way to go about it. I don't know enough
  * about C and linking to understand whether the need for dlopen is due
  * to injecting the library into a running process or if it's just a side
  * effect of building the library the way I do here.
  *
  * I'm also not sure how to properly statically link subhook into the
  * resulting dynamic library created in this project (or if that would
  * even solve this dlopen/dlsym requirement).
  *
  * As a result we have some hacks like get_symbol_by_name and the dlopen/dlsym
  * used below.
  */
__attribute__((constructor))
void initHook() {
  // First we load up the subhook library.
  // NOTE: Hardcoded path which assumes you're injecting from a parent directory of subhook.
  void* subhook_lib = dlopen("subhook/libsubhook.so",  RTLD_LAZY | RTLD_GLOBAL);
  if (!subhook_lib) {
      printf("Could not find subhook/libsubhook.so\n");
      exit(1);
  }
  // Load up the subhook_new function from the subhook shared library.
  subhook_t (*subhook_new)(void *, void *, subhook_flags_t) = dlsym(subhook_lib, "subhook_new");
  // Load up the subhook_install function from the subhook shared library.
  int (*subhook_install)(subhook_t) = dlsym(subhook_lib, "subhook_install");
  // Load up the subhook_remove function from the subhook shared library.
  //int (*subhook_remove)(subhook_t) = dlsym(subhook_lib, "subhook_remove");
  // Load up the subhook_free function from the subhook shared library.
  //int (*subhook_free)(subhook_t) = dlsym(subhook_lib, "subhook_free");
  if (!subhook_new || !subhook_install) {
    fprintf(stderr, "Could not get subhook_new/install function pointers: %s\n", dlerror());
    return;
  }
  // Initialize and install our hook.
  hook = subhook_new(get_symbol_by_name("Platform_getUptime"), (void *)Platform_getUptime_Hook, 1); // 0 == 32bit, 1 == 64bit
  subhook_install(hook);
}

/**
  * We can optionally call the destructor to clean-up when library unloads.
  */
__attribute__((destructor))
void cleanUpHook() {
    subhook_remove(hook);
    subhook_free(hook);
}