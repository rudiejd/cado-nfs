#include "cado.h" // IWYU pragma: keep
#include "utils_int64.h"
#include "macros.h"

int main()
{
  ASSERT_ALWAYS(554343025 == invmod_uint64(324937894, 650109353));
  ASSERT_ALWAYS(299617373 == invmod_uint64(495120181, 585996241));
  ASSERT_ALWAYS(122651190 == invmod_uint64(504807929, 990446713));
  ASSERT_ALWAYS(232379205 == invmod_uint64(51116435, 399068249));
  /*ASSERT_ALWAYS(7806613 == invmod_uint64(748791969, 8871761));*/
  /*ASSERT_ALWAYS(19840543 == invmod_uint64(696913899, 21729739));*/
  /*ASSERT_ALWAYS(51622536 == invmod_uint64(489964388, 730821473));*/
  /*ASSERT_ALWAYS(333869966 == invmod_uint64(411658668, 839030107));*/
  /*ASSERT_ALWAYS(18270235 == invmod_uint64(376422880, 31358231));*/
  /*ASSERT_ALWAYS(145243839 == invmod_uint64(505136977, 180253027));*/

  /*ASSERT_ALWAYS(572188830345158763 == invmod_uint64(906831737512135180,*/
        /*677230092143790439));*/
  /*ASSERT_ALWAYS(169954208220019684 == invmod_uint64(567096807813590478,*/
        /*463785648551612053));*/
  /*ASSERT_ALWAYS(163023602895957372 == invmod_uint64(74423861502025396,*/
        /*225554272371934393));*/
  /*ASSERT_ALWAYS(135872834976928493 == invmod_uint64(655005633019142874,*/
        /*1125289645204482133));*/
  /*ASSERT_ALWAYS(4146000131811539 == invmod_uint64(851032067979284930,*/
        /*334503327832179661));*/
  /*ASSERT_ALWAYS(346400682792454667 == invmod_uint64(936449143567536659,*/
        /*514438339173759353));*/
  /*ASSERT_ALWAYS(933745066896340953 == invmod_uint64(894685551665666663,*/
        /*1015534740548624701));*/
  /*ASSERT_ALWAYS(927691403012340799 == invmod_uint64(1029308886095189222,*/
        /*1142552716345275931));*/
  /*ASSERT_ALWAYS(376883205168091117 == invmod_uint64(411181756706293570,*/
        /*620458578567669367));*/
  /*ASSERT_ALWAYS(46044753533828937 == invmod_uint64(155363330226115859,*/
        /*75814897122147647));*/

  return 0;
}
