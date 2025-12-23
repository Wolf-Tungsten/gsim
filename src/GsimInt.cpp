#include "GsimInt.h"

// Explicit instantiations to avoid per-TU template bloat for common widths.
#if GSIMINT_EXTERN_TEMPLATES
namespace gsim {

#define GSIMINT_INSTANTIATE_ONE(WIDTH) \
  template class GsimInt<WIDTH, false>; \
  template class GsimInt<WIDTH, true>;
GSIMINT_EXTERN_WIDTH_LIST(GSIMINT_INSTANTIATE_ONE)
#undef GSIMINT_INSTANTIATE_ONE

} // namespace gsim
#endif

