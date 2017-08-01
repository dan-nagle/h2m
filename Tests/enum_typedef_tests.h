// This is a simple test of h2m functionality.
// It should compile without complaint.

enum my_stuff {onething, twothing, threething};

enum their_stuff {thingone, thingtwo = 15};

typedef enum my_stuff my_stuff_t;

typedef int* integer_pointer;

int takes_type_def (enum their_stuff an_arg, enum my_stuff another_arg);

struct little_struct {
  int a;
  double b;
};

typedef struct little_struct other;

int takes_other(other my_other, my_stuff_t b, integer_pointer c);

