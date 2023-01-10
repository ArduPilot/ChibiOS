/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
/*
    This module is based on the work of John Walker (April of 1989) and
    merely adapted to work in ChibiOS. The author has not specified
    additional license terms so this is released using the most permissive
    license used in ChibiOS. The license covers the changes only, not the
    original work.
 */

#ifdef __cplusplus
extern "C" {
#endif
  void fourn_float(float data[], int nn[], const int ndim, const int isign);
  void fourn_double(double data[], int nn[], const int ndim, const int isign);
#ifdef __cplusplus
}
#endif
