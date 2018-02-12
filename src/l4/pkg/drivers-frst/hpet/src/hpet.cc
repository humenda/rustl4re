/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/hpet>

#include <stdio.h>

void
L4::Driver::Hpet::print_state() const
{
  printf("HPET Information:\n");
  printf("Rev-id:               0x%x\n", rev_id());
  printf("Num timers:           %d\n", num_tim_cap());
  printf("Count size:           %s\n", count_size_cap() ? "64bit" : "32bit");
  printf("Legacy capable:       %s\n", leg_rt_cap() ? "Yes" : "No");
  printf("Legacy enabled:       %s\n", leg_rt_cnf() ? "Yes" : "No");
  printf("Vedor ID:             0x%x\n", vendor_id());
  printf("Counter clock period: 0x%x / %u\n", counter_clk_period(),
                                              counter_clk_period());
  printf("Frequency (Hz):       %lld\n", 1000000000000000ULL / counter_clk_period());

  printf("Enabled:              %s\n", enabled() ? "Yes" : "No");

  printf("Main counter value:   %lld\n", main_counter_val());

  printf("Raw values:           %16llx %16llx\n", cap_and_id(), conf());


  for (unsigned i = 0; i < num_tim_cap(); ++i)
    {
      printf("HPET Timer-%d Information:\n", i);
      timer(i)->print_state();
    }
}

void
L4::Driver::Hpet::Timer::print_state() const
{
  printf(" Int type:             %s\n", is_int_type_level() ? "Level" : "Edge");
  printf(" IRQ enabled:          %s\n", is_int_enabled() ? "Yes" : "No");
  printf(" Mode:                 %s\n", is_periodic() ? "Periodic" : "Non-periodic");
  printf(" Periodic int capable: %s\n", periodic_int_capable() ? "Yes" : "No");
  printf(" Can 64bit:            %s\n", can_64bit() ? "Yes" : "No");
  printf(" Forced to 32bit:      %s\n", forced_32bit() ? "Yes" : "No");
  printf(" Can FSB/MSI:          %s\n", can_fsb() ? "Yes" : "No");
  printf(" Does FSB/MSI:         %s\n", is_fsb() ? "Yes" : "No");
  printf(" IRQs available:       %x\n", ints_avail());
  printf(" IRQ set:              %d\n", int_route_cnf());
  printf(" Comparator value:     %lld\n", comparator());
  printf(" Raw values:           %16llx %16llx\n", conf_and_cap(), comp());
}

