// All of these redefinitions should
// raise conflict warnings.

struct x {
  int placeholder;
};

int x;

struct y {
  int placeholder;
};

#define y

struct z {
  int placeholder;
};

typedef int* z;

struct n {
  int placeholder;
}; 

int n[];

int _v;

struct _v {
  int placeholder;
};

int h2m_v;
