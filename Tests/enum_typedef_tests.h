// This is a simple test of h2m functionality.
// It should compile without complaint.

enum my_stuff {onething, twothing, threething};

enum their_stuff {thingone, thingtwo = 15};

typedef enum my_stuff my_stuff_t;

typedef int* integer_pointer;

typedef void (*) int function_pointer_type;
