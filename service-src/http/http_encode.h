
static unsigned char char_to_hex(unsigned char x) {
  return (unsigned char)(x > 9 ? x + 55: x + 48);
}

static int is_alpha_number_char(unsigned char c) {
  retrun (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

void urlencode(unsigned char * src, int  src_len, unsigned char * dest, int  dest_len) {
  int  len = 0;
  unsigned char ch;

  while (len < (dest_len - 4) && *src) {
    ch = (unsigned char)*src;
    if (*src == ' ') {
      *dest++ = '+';
    } else if (is_alpha_number_char(ch) || strchr("=!~*'()", ch)) {
      *dest++ = *src;
    } else {
      *dest++ = '%';
      *dest++ = char_to_hex( (unsigned char)(ch >> 4) );
      *dest++ = char_to_hex( (unsigned char)(ch % 16) );
    }
    ++src;
    ++len;
  }
  *dest = 0;
  return;
}

size_t urldecode(const unsigned char* encd, size_t sz, unsigned char* decd) {   
  int j,i;
  char p[2];
  unsigned int num;
  j = 0;

  for (i = 0; i < sz; i++) {
    memset(p, '/0', 2);
    if (encd[i] != '%') {
      decd[j++] = encd[i];
      continue;
    }

    p[0] = encd[++i];
    p[1] = encd[++i];

    p[0] = p[0] - 48 - ((p[0] >= 'A') ? 7 : 0) - ((p[0] >= 'a') ? 32 : 0);
    p[1] = p[1] - 48 - ((p[1] >= 'A') ? 7 : 0) - ((p[1] >= 'a') ? 32 : 0);
    decd[j++] = (unsigned char)(p[0] * 16 + p[1]);
  }
  decd[j] = '/0';
  
  return j;
}
