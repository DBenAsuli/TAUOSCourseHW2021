//
// Created by Dvir Ben Asuli on 03/11/2020.
// Tel-Aviv University, Operating Systems Course Assignement 1, 2021a
//
#include "os.h"

/* Declerations */
/**
 * @purpose Create or destroy virtual memory mappings in a page table
 * @param pt => The physical page number of the page table root
 *              aka the physical page that the page table base register in the CPU state will point to)
 * @param vpn => The virtual page number the caller wishes to map/unmap.
 * @param ppn => Can be one of two cases. If ppn is equal to a special NO MAPPING value then vpn’s
 *               mapping (if exists) should be destroyed.
 *               Otherwise, ppn specifies the physical page number that vpn should be mapped to.
 * @return Nothing
 */
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn);

/**
 * @purpose Query the mapping of a virtual page number in a page table
 * @param pt => The physical page number of the page table root
 *              aka the physical page that the page table base register in the CPU state will point to)
 * @param vpn => The Virtual Page Number we want to query
 *
 * @return The physical page number the vpn is mapped to, or NO MAPPING if no mapping exists.
 */
uint64_t page_table_query(uint64_t pt, uint64_t vpn);

/* Enums */

/* Function Implementations */

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
    uint64_t nine_bits_msb = (~((1ULL <<36)-1) & ((1ULL <<45)-1)); //For masking 9 bits of VPN Symbol
    uint64_t *pt_node = phys_to_virt((pt << 12));
    uint64_t twelve_bits_lsb = ~((1ULL <<12)-1) ; // 12 zeros at LSB to Cancel Offset in input to phys_to_virt
    uint64_t temp_vpn = vpn; //Will be changed s.t. bits 45-36 will always be current symbol
    uint64_t curr_symbol = (((temp_vpn & nine_bits_msb) >> 36)) ; // Keeping only leftmost 9 bits and shifting it to the right
    uint64_t new_page_add;
    uint64_t final_pte;

    for (int layer = 1; layer < 5; layer++) {
        if (pt_node[curr_symbol] == 0) {
            new_page_add = alloc_page_frame();
            new_page_add = (new_page_add << 12) + 1 ; //Padding with 12 bits offset and valid bit
            pt_node[curr_symbol] = new_page_add;

        } else {
            new_page_add = pt_node[curr_symbol];
        }

        pt_node = phys_to_virt(new_page_add & twelve_bits_lsb);
        temp_vpn = temp_vpn << 9;
        curr_symbol = (((temp_vpn & nine_bits_msb) >> 36));
    }

    if (ppn == NO_MAPPING){
        final_pte = pt_node[curr_symbol];
        final_pte = ((final_pte >> 1) << 1); //Turning valid bit of the pte to 0, destroying it
    } else {
        final_pte = (ppn << 12) + 1;
    }

    pt_node[curr_symbol] = final_pte;

}

uint64_t page_table_query(uint64_t pt, uint64_t vpn){
    uint64_t nine_bits_msb = (~((1ULL <<36)-1) & ((1ULL <<45)-1));
    uint64_t twelve_bits_lsb = ~((1ULL <<12)-1) ; // 12 zeros at LSB
    uint64_t *pt_node = phys_to_virt((pt << 12));
    uint64_t temp_vpn = vpn;
    uint64_t curr_symbol = (((temp_vpn & nine_bits_msb) >> 36)) ; // Keeping only leftmost 9 bits and shifting it to the right
    uint64_t next_page_add;
    uint64_t final_pte;

    for (int layer = 1; layer < 5; layer++) {
        next_page_add = pt_node[curr_symbol];
        if ((next_page_add & 1LLU) == 0) {
            return NO_MAPPING;
        }

        pt_node = phys_to_virt(next_page_add & twelve_bits_lsb);
        temp_vpn = temp_vpn << 9;
        curr_symbol = (((temp_vpn & nine_bits_msb) >> 36));
    }

    final_pte = pt_node[curr_symbol];

    if ((final_pte & 1LLU) == 0) {
        return NO_MAPPING;
    } else {
        final_pte = final_pte >> 12; //offset
        return final_pte;
    }
}
