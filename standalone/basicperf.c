/* -*- Mode: C -*- */

#include <pci.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <mbi-tools.h>

static void
t_empty(void)
{
}

static void
t_vmcall(void)
{
  asm volatile ("vmcall" : : "a" (0));
}

static void
t_cpuid(void)
{
  uint32_t eax = 0;
  asm volatile ("cpuid" : "+a" (eax) :: "ecx", "edx", "ebx");
}

static void
t_portio(void)
{
  inb(0x60);                    /* keyboard */
}

static void
t_mmio(void)
{
  ((volatile uint32_t *)0xFEE00000)[1]; /* APIC reg 1 */
}

struct test {
  const char *name;
  void (*test_fn)(void);
};

static const struct test tests[] = {
  { "empty ",  t_empty },
  { "vmcall", t_vmcall },
  { "cpuid ", t_cpuid },
  { "portio", t_portio },
  { "mmio  ", t_mmio },
};

static float sqrtf(float v)
{
  asm ("fsqrt" : "+t" (v));
  return v;
}

int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();

  if (magic != MBI_MAGIC) {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nbasicperf %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  static const  unsigned tries = 1024;
  static uint32_t results[1024];
  memset(results, 0, sizeof(results)); /* Warmup */

  for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    unsigned retries = 0;
    uint64_t start, end;

  again:
    for (unsigned j = 0; j < tries; j++) {
        start = rdtsc();
        tests[i].test_fn();
        end = rdtsc();
        results[j] = end - start;
    }

    uint64_t sum = 0;
    for (unsigned j = 0; j < tries; j++) sum += results[j];
    float mean = (float)sum / tries;

    float sqdiff = 0;
    for (unsigned j = 0; j < tries; j++) sqdiff += (mean - results[j])*(mean - results[j]);
    float stddev = sqrtf(sqdiff/tries);

    if ((retries++ < 5) && (stddev > 2000)) {
      printf("Retry test %s because of instability: stddev %u\n", tests[i].name, (uint32_t)stddev);
      goto again;
    }
    
    printf("Test %s: retries %u mean %u stddev %u\n", tests[i].name, retries, (uint32_t)mean, (uint32_t)stddev);
  }

  return 0;
}
