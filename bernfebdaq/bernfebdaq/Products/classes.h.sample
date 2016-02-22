#include "art/Persistency/Common/Wrapper.h"
#include "artdaq-demo/Products/CompressedV172x.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-demo/Products/Channel.hh"

#include <vector>

namespace {
  struct dictionary {
    demo::CompressedV172x d1;
  };
}

template class art::Wrapper<demo::CompressedV172x>;

template class std::vector<darkart::Channel>;
template class art::Wrapper<std::vector<darkart::Channel> >;
template class art::Wrapper<darkart::Channel>;

