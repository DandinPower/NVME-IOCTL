#pragma once
#include <config.h>
#include <utils.h>
#include <stdlib.h> // for system()
#include <cmath> // for ceil()
#include <stdio.h> //for sprintf()
#include <iostream> // for std::cout, std::endl

/**
 * @brief Reserves a number of huge pages in the system.
 *
 * This function calculates the number of huge pages needed based on the reserve_size
 * and the HUGE_PAGE_SIZE. It then uses a system command to reserve these pages.
 *
 * @param reserve_size The size of memory to reserve in bytes.
 */
void reserve_system_huge_page(int reserve_size) {
    assert_msg(HUGE_PAGE_STRATEGY == 1, "HUGE_PAGE_STRATEGY is not enabled", true);
    int reserve_page_number = std::ceil(static_cast<double>(reserve_size) / static_cast<double>(HUGE_PAGE_SIZE));
    char command[100];
    sprintf(command, "sudo sysctl vm.nr_hugepages=%d", reserve_page_number);
    system(command);
}

/**
 * @brief Reverts the system huge page reservation.
 *
 * This function uses a system command to set the number of reserved huge pages to 0,
 * effectively reverting any previous reservation.
 */
void revert_system_huge_page() {
    assert_msg(HUGE_PAGE_STRATEGY == 1, "HUGE_PAGE_STRATEGY is not enabled", true);
    system("sudo sysctl vm.nr_hugepages=0");
}