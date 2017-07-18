#include "c_writer_helper.h"

size_t data_write(void *ptr, size_t write_size, size_t nmemb, void *stream) {

  size_t written = fwrite(ptr, write_size, nmemb, (FILE *) stream);
  return written;
}
