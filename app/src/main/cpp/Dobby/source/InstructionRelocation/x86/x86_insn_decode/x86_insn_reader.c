/* Specialized instruction reader. */
typedef struct x86_insn_reader_t {
  const unsigned char *prefix; /* pointer to beginning of instruction */
  const unsigned char *opcode; /* pointer to opcode */
  const unsigned char *modrm;  /* pointer to modrm byte */

  unsigned char        buffer[20];    /* buffer used when few bytes left */
  const unsigned char *buffer_cursor; /* pointer to buffer_cursor of insn + 1 */
} x86_insn_reader_t;

/* Initializes a bytecode reader to read code from a given part of memory. */
static void init_reader(x86_insn_reader_t *rd, const unsigned char *begin, const unsigned char *buffer_cursor) {
  if (buffer_cursor - begin < sizeof(rd->buffer)) {
    memset(rd->buffer, 0xcc, sizeof(rd->buffer)); /* debug token */
    memcpy(rd->buffer, begin, buffer_cursor - begin);
    rd->prefix = rd->buffer;
  } else {
    rd->prefix = begin;
  }
  rd->opcode = rd->modrm = rd->buffer_cursor = rd->prefix;
}

uint32_t reader_offset(x86_insn_reader_t *rd) {
  return rd->buffer_cursor - rd->buffer;
}

static uint8_t peek_byte(const x86_insn_reader_t *rd) {
  return *rd->buffer_cursor;
}

#define read_uint8 read_byte
static uint8_t read_byte(x86_insn_reader_t *rd) {
  DLOG(0, "[x86 insn reader] %p - 1", rd->buffer_cursor);

  const unsigned char *p = rd->buffer_cursor;
  rd->buffer_cursor++;
  return *p;
}

#define read_uint16 read_word
static uint16_t read_word(x86_insn_reader_t *rd) {
  DLOG(0, "[x86 insn reader] %p - 2", rd->buffer_cursor);

  const unsigned char *p = rd->buffer_cursor;
  rd->buffer_cursor += 2;
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

#define read_uint32 read_dword
static uint32_t read_dword(x86_insn_reader_t *rd) {
  DLOG(0, "[x86 insn reader] %p - 4", rd->buffer_cursor);

  const unsigned char *p = rd->buffer_cursor;
  rd->buffer_cursor += 4;
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define read_uint64 read_qword
static uint64_t read_qword(x86_insn_reader_t *rd) {
  DLOG(0, "[x86 insn reader] %p - 8", rd->buffer_cursor);

  uint64_t *p = (uint64_t *)rd->buffer_cursor;
  rd->buffer_cursor += 4;
  return p[0];
}

static uint32_t read_imm(x86_insn_reader_t *rd, int size) {
  DLOG(0, "[x86 insn reader] %p", rd->buffer_cursor);

  return (size == 8) ? read_byte(rd) : (size == 16) ? read_word(rd) : (size == 32) ? read_dword(rd) : 0;
}

static unsigned char read_modrm(x86_insn_reader_t *rd) {
  if (rd->buffer_cursor == rd->modrm)
    rd->buffer_cursor++;
  return *rd->modrm;
}

/* Marks the next byte as ModR/M. */
static void continue_modrm(x86_insn_reader_t *rd) {
  rd->modrm = rd->buffer_cursor;
}

/* Marks the next byte as opcode. */
static void continue_opcode(x86_insn_reader_t *rd) {
  rd->modrm = rd->opcode = rd->buffer_cursor;
}
