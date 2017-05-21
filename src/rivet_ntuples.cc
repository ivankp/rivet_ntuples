#include <iostream>
#include <cstring>
#include <vector>

#include <boost/optional.hpp>

#include <TFile.h>
#include <TChain.h>
#include <TKey.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>

#include <HepMC/GenEvent.h>
#include <Rivet/AnalysisHandler.hh>

#include "timed_counter.hh"
#include "float_or_double_reader.hh"

#define test(var) \
  std::cout <<"\033[36m"<< #var <<"\033[0m"<< " = " << var << std::endl;

using std::cout;
using std::cerr;
using std::endl;

template <typename T, typename F>
auto operator%(const std::vector<T>& v, F&& f) {
  std::vector<decltype(f(std::declval<const T&>()))> out;
  out.reserve(v.size());
  for (const auto& x : v) out.emplace_back(f(x));
  return std::move(out);
}
template <typename T, typename F>
auto operator%(std::vector<T>& v, F&& f) {
  std::vector<decltype(f(std::declval<T&>()))> out;
  out.reserve(v.size());
  for (auto& x : v) out.emplace_back(f(x));
  return std::move(out);
}

// https://github.com/danielmaitre/nTuple2Rivet/blob/master/nTuple2Rivet.cpp
// https://github.com/danielmaitre/nTupleReader/blob/master/nTupleReader_impl.cpp#L214

int main(int argc, char* argv[]) {
  if (argc<4) {
    cout << "usage: " << argv[0] << " rivet_analysis rivet_output ntuple.root"
     " [(ntuple|weights).root]" << endl;
    return 1;
  }

  std::vector<std::unique_ptr<TFile>> files;

  // Open input ntuples root file ===================================
  TChain chain("t3");
  boost::optional<TChain> weights_chain;
  cout << "\033[36mInput files\033[0m:" << endl;
  for (int i=3; i<argc; ++i) {
    cout << "  " << argv[i];
    TFile *f = new TFile(argv[i]);
    files.emplace_back(f);
    if (f->IsZombie()) return 1;
    TKey *tree_key = nullptr;
    if ((tree_key = f->FindKey("t3"))
        && !strcmp(tree_key->GetClassName(),"TTree")) {
      chain.AddFile(argv[i],0);
      cout << " (ntuple)";
    } else if ((tree_key = f->FindKey("weights"))
        && !strcmp(tree_key->GetClassName(),"TTree")) {
      if (!weights_chain) weights_chain.emplace("weights");
      weights_chain->AddFile(argv[i],0);
      cout << " (weights)";
    }
    cout << endl;
  }
  if (weights_chain) chain.AddFriend(&*weights_chain);
  cout << endl;

  // Set up branches for reading
  TTreeReader reader(&chain);

  TTreeReaderValue<Int_t> _id(reader,"id");
  TTreeReaderValue<Int_t> _nparticle(reader,"nparticle");
  TTreeReaderArray<Int_t> _kf(reader,"kf");

  float_or_double_array_reader _px(reader,"px");
  float_or_double_array_reader _py(reader,"py");
  float_or_double_array_reader _pz(reader,"pz");
  float_or_double_array_reader _E (reader,"E" );

  boost::optional<TTreeReaderValue<Int_t>> _ncount;
  for ( auto bo : *reader.GetTree()->GetListOfBranches() ) {
    if (!strcmp(bo->GetName(),"ncount")) _ncount.emplace(reader,"ncount");
  }

  // handle multiple weights
  std::vector<float_or_double_value_reader> _weights;
  if (!weights_chain) _weights.emplace_back(reader,"weight");
  else {
    const TObjArray *bb = weights_chain->GetListOfBranches();
    _weights.reserve(bb->GetEntriesFast());
    for (const auto* b : *bb) {
      _weights.emplace_back(reader,static_cast<const TBranch*>(b)->GetName());
    }
  }
  // std::

  Rivet::AnalysisHandler rivet;
  rivet.addAnalysis(argv[1]);
  if (rivet.analysisNames().size() != 1) {
    std::cerr << "No analysis " << argv[1] << " found!"
      " Try setting RIVET_ANALYSIS_PATH" << std::endl;
    return 1;
  }
  cout << "\033[36mRivet analyses\033[0m:" << endl;
  for (const auto& name : rivet.analysisNames())
    cout << "  " << name << endl;
  cout << endl;

  
  // LOOP ===========================================================
  using tc = ivanp::timed_counter<Long64_t>;
  for (tc ent(reader.GetEntries(true)); reader.Next(); ++ent) {
    using namespace HepMC;

    GenVertex* v = new GenVertex();
    GenParticle
      *gp1 = new GenParticle(6500, 2212, 4),
      *gp2 = new GenParticle(6500, 2212, 4);
    v->add_particle_in(gp1);
    v->add_particle_in(gp2);

    GenEvent event(
      Units::GEV, Units::MM,
      0, /*signal_process_id*/
      *_id, /*event_number*/
      v, /*signal_vertex*/
      _weights % [](auto& w){ return *w; } /*weights*/
    );
    event.set_beam_particles(gp1,gp2);

    for (int i=0, n=*_nparticle; i<n; ++i) {
      v->add_particle_out(
        new GenParticle({_px[i],_py[i],_pz[i],_E[i]}, _kf[i], 1));
      // p->set_generated_mass(0);
    }

    rivet.analyze(event);
  } // END LOOP

  rivet.finalize();
  rivet.writeData(argv[2]);

  return 0;
}
