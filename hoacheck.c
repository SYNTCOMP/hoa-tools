/**************************************************************************
 * Copyright (c) 2019- Guillermo A. Perez
 * 
 * This file is part of HOATOOLS.
 * 
 * HOATOOLS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * HOATOOLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with HOATOOLS. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Guillermo A. Perez
 * University of Antwerp
 * guillermoalberto.perez@uantwerpen.be
 *************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "simplehoa.h"

int main(int argc, char* argv[]) {
    HoaData data;
    defaultsHoa(&data);
    int ret = parseHoa(stdin, &data);

    if (ret != 0)
        return ret;

#ifndef NDEBUG
    printHoa(&data);
#endif
    
    bool isMaxParity;
    short resGoodPriority;
    ret = isParityGFG(&data, &isMaxParity, &resGoodPriority);
    if (ret != 0)
        return ret;

    resetHoa(&data);
    return EXIT_SUCCESS;
}
