//
//  Copyright (C) 2014 Greg Landrum
//  Adapted from pseudo-code from Roger Sayle
//
//   @@ All Rights Reserved @@
//  This file is part of the RDKit.
//  The contents are covered by the terms of the BSD license
//  which is included in the file license.txt, found at the root
//  of the RDKit source tree.
//

#include <GraphMol/ROMol.h>
#include <GraphMol/RingInfo.h>
#include <boost/cstdint.hpp>
#include <boost/foreach.hpp>
#include <boost/dynamic_bitset.hpp>
#include <cstring>
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

// #define VERBOSE_CANON 1

namespace RDKit {
  namespace Canon{

    struct bondholder {
      Bond::BondType bondType;
      unsigned int bondStereo;
      unsigned int nbrSymClass;
      unsigned int nbrIdx;
      bondholder() : bondType(Bond::UNSPECIFIED),
          bondStereo(static_cast<unsigned int>(Bond::STEREONONE)),
          nbrIdx(0), nbrSymClass(0) {};
      bondholder(Bond::BondType bt,Bond::BondStereo bs,unsigned int ni,unsigned int nsc) :
        bondType(bt),
        bondStereo(static_cast<unsigned int>(bs)),
        nbrIdx(ni),
        nbrSymClass(nsc) {};
      bondholder(Bond::BondType bt,unsigned int bs,unsigned int ni,unsigned int nsc) :
        bondType(bt),
        bondStereo(bs),
        nbrIdx(ni),
        nbrSymClass(nsc) {};
      bool operator<(const bondholder &o) const {
        if(bondType!=o.bondType)
          return bondType<o.bondType;
        if(bondStereo!=o.bondStereo)
          return bondStereo<o.bondStereo;
        return nbrSymClass<o.nbrSymClass;
      }
      static bool greater(const bondholder &lhs,const bondholder &rhs){
        if(lhs.bondType!=rhs.bondType)
          return lhs.bondType>rhs.bondType;
        if(lhs.bondStereo!=rhs.bondStereo)
          return lhs.bondStereo>rhs.bondStereo;
        return lhs.nbrSymClass>rhs.nbrSymClass;
      }

      static int compare(const bondholder &x,const bondholder &y,unsigned int div=1){
        if(x.bondType<y.bondType)
          return -1;
        else if(x.bondType>y.bondType)
          return 1;
        if(x.bondStereo<y.bondStereo)
          return -1;
        else if(x.bondStereo>y.bondStereo)
          return 1;

        return x.nbrSymClass/div-y.nbrSymClass/div;
      }
    };

    struct canon_atom{
      const Atom *atom;
      int index;
      unsigned int degree;
      unsigned int totalNumHs;
      unsigned int numRingMember;
      bool hasRingNbr;
      bool isRingStereoAtom;
      int* nbrIds;
      const std::string *p_symbol; // if provided, this is used to order atoms
      std::vector<bondholder> bonds;

      canon_atom() : atom(NULL),index(-1),degree(0),totalNumHs(0),numRingMember(0),
          hasRingNbr(false), isRingStereoAtom(false), nbrIds(NULL), p_symbol(NULL) {};
    };

    template <typename CompareFunc>
    bool hanoi( int* base, int nel, int *temp, int *count, int *changed, CompareFunc compar ) {
      //std::cerr<<"  hanoi: "<<nel<<std::endl;
      register int *b1,*b2;
      register int *t1,*t2;
      register int *s1,*s2;
      register int n1,n2;
      register int result;
      register int *ptr;

      if( nel == 1 ) {
        count[base[0]] = 1;
        return false;
      } else if( nel == 2 ) {
        n1 = base[0];
        n2 = base[1];
        int stat = (/*!changed || */changed[n1]||changed[n2]) ? compar(n1,n2) : 0;
        if( stat == 0 ) {
          count[n1] = 2;
          count[n2] = 0;
          return false;
        } else if( stat < 0 ) {
          count[n1] = 1;
          count[n2] = 1;
          return false;
        } else /* stat > 0 */ {
          count[n1] = 1;
          count[n2] = 1;
          base[0] = n2;   /* temp[0] = n2; */
          base[1] = n1;   /* temp[1] = n1; */
          return false;   /* return True;  */
        }
      }

      n1 = nel/2;    n2 = nel-n1;
      b1 = base;     t1 = temp;
      b2 = base+n1;  t2 = temp+n1;

      if( hanoi(b1,n1,t1,count,changed,compar) ) {
        if( hanoi(b2,n2,t2,count,changed,compar) ) {
          s2 = t2;
        } else s2 = b2;
        result = false;
        ptr = base;
        s1 = t1;
      } else {
        if( hanoi(b2,n2,t2,count,changed,compar) ) {
          s2 = t2;
        } else s2 = b2;
        result = true;
        ptr = temp;
        s1 = b1;
      }

      while( true ) {
        assert(*s1!=*s2);
        int stat = (/*!changed || */changed[*s1]||changed[*s2]) ? compar(*s1,*s2) : 0;
        int len1 = count[*s1];
        int len2 = count[*s2];
        assert(len1>0);
        assert(len2>0);
        if( stat == 0 ) {
          count[*s1] = len1+len2;
          count[*s2] = 0;
          memmove(ptr,s1,len1*sizeof(int));
          ptr += len1;  n1 -= len1;
          if( n1 == 0 ) {
            if( ptr != s2 )
              memmove(ptr,s2,n2*sizeof(int));
            return result;
          }
          s1 += len1;

          //std::cerr<<"  cpy: "<<*s1<<" "<<*s2<<" "<<len2<<std::endl;
          memmove(ptr,s2,len2*sizeof(int));
          ptr += len2; n2 -= len2;
          if( n2 == 0 ) {
            memmove(ptr,s1,n1*sizeof(int));
            return result;
          }
          s2 += len2;
        } else if( stat < 0 && len1>0 ) {
          memmove(ptr,s1,len1*sizeof(int));
          ptr += len1;  n1 -= len1;
          if( n1 == 0 ) {
            if( ptr != s2 )
              memmove(ptr,s2,n2*sizeof(int));
            return result;
          }
          s1 += len1;
        } else if (stat > 0  && len2>0) /* stat > 0 */ {
          memmove(ptr,s2,len2*sizeof(int));
          ptr += len2; n2 -= len2;
          if( n2 == 0 ) {
            memmove(ptr,s1,n1*sizeof(int));
            return result;
          }
          s2 += len2;
        } else {
          assert(0);
        }
      }
    }

    template <typename CompareFunc>
    void hanoisort( int* base, int nel, int *count, int *changed, CompareFunc compar )
    {
      register int *temp;

      temp = (int*)malloc(nel*sizeof(int));
      if( hanoi(base,nel,temp,count,changed,compar) )
        memmove(base,temp,nel*sizeof(int));
      free(temp);
    }

    void CreateSinglePartition(unsigned int nAtoms,
                               int *order,
                               int *count,
                               canon_atom *atoms);

    void ActivatePartitions(unsigned int nAtoms,
                            int *order,
                            int *count,
                            int &activeset,int *next,
                            int *changed);

    class AtomCompareFunctor {
      void getAtomNeighborhood(unsigned int i,std::vector<bondholder> &nbrs) const{
        if(dp_atomsInPlay && !(*dp_atomsInPlay)[i])
          return;
        for(unsigned j=0; j < nbrs.size(); ++j){
          unsigned newSymClass = dp_atoms[nbrs.at(j).nbrIdx].index;
          nbrs.at(j).nbrSymClass = newSymClass;
        }
        std::sort(nbrs.begin(),nbrs.end(),bondholder::greater);
      }

      unsigned int getAtomRingNbrCode(unsigned int i) const {
        if(!dp_atoms[i].hasRingNbr) return 0;

        const Atom *at=dp_atoms[i].atom;
        int *nbrs = dp_atoms[i].nbrIds;
        for(unsigned j=0; j<dp_atoms[i].degree; ++j){
          if(dp_atoms[dp_atoms[i].nbrIds[j]].isRingStereoAtom){
            return dp_atoms[dp_atoms[i].nbrIds[j]].index*1000000+j;
          }
        }
        return 0;
      }

      int basecomp(int i,int j,int mode) const {
        PRECONDITION(dp_atoms,"no atoms");
        unsigned int ivi,ivj;

        // always start with the current class:
        ivi= dp_atoms[i].index;
        ivj= dp_atoms[j].index;
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // start by comparing degree
        ivi= dp_atoms[i].degree;
        ivj= dp_atoms[j].degree;
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        if(dp_atoms[i].p_symbol &&
            dp_atoms[j].p_symbol ) {
          if( *(dp_atoms[i].p_symbol) < *(dp_atoms[j].p_symbol) )
            return -1;
          else if( *(dp_atoms[i].p_symbol) > *(dp_atoms[j].p_symbol) )
            return 1;
          else
            return 0;
        }
        // move onto atomic number
        ivi= dp_atoms[i].atom->getAtomicNum();
        ivj= dp_atoms[j].atom->getAtomicNum();
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // isotopes if we're using them
        if(df_useIsotopes){
          ivi=dp_atoms[i].atom->getIsotope();
          ivj=dp_atoms[j].atom->getIsotope();
          if(ivi<ivj)
            return -1;
          else if(ivi>ivj)
            return 1;
        }

        // nHs
        ivi=dp_atoms[i].totalNumHs;
        ivj=dp_atoms[j].totalNumHs;
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // charge
        ivi=dp_atoms[i].atom->getFormalCharge();
        ivj=dp_atoms[j].atom->getFormalCharge();
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // ring membership
        // initial passes at this were just checking "isInRing" to allow
        // a possible future more efficient check. These break on this
        // lovely double-diamond pathological case:
        //   *12*3*1*3*4*5*4*52
        //
        // probably going to regret allowing this to be skipped some day
        ivi=dp_atoms[i].numRingMember;
        ivj=dp_atoms[j].numRingMember;
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;


        // chirality if we're using it
        if(df_useChirality){
          // first atom stereochem:
          ivi=0;
          ivj=0;
          std::string cipCode;
          if(dp_atoms[i].atom->getPropIfPresent(common_properties::_CIPCode,cipCode)){
            ivi=cipCode=="R"?2:1;
          }
          if(dp_atoms[j].atom->getPropIfPresent(common_properties::_CIPCode,cipCode)){
            ivj=cipCode=="R"?2:1;
          }
          if(ivi<ivj)
            return -1;
          else if(ivi>ivj)
            return 1;

          // can't actually use values here, because they are arbitrary
          ivi=dp_atoms[i].atom->getChiralTag()!=0;
          ivj=dp_atoms[j].atom->getChiralTag()!=0;
          if(ivi<ivj)
            return -1;
          else if(ivi>ivj)
            return 1;

        }
        if(df_useChiralityRings){
          // ring stereochemistry
          if(dp_atoms[i].numRingMember && dp_atoms[j].numRingMember){
            ivi=getAtomRingNbrCode(i);
            ivj=getAtomRingNbrCode(j);
            if(ivi<ivj)
              return -1;
            else if(ivi>ivj)
              return 1;
          }
          // bond stereo is taken care of in the neighborhood comparison
        }
        return 0;
      }
    public:
      Canon::canon_atom *dp_atoms;
      const ROMol *dp_mol;
      const boost::dynamic_bitset<> *dp_atomsInPlay,*dp_bondsInPlay;
      bool df_useNbrs;
      bool df_useIsotopes;
      bool df_useChirality;
      bool df_useChiralityRings;

      AtomCompareFunctor() : dp_atoms(NULL), dp_mol(NULL),
                             dp_atomsInPlay(NULL), dp_bondsInPlay(NULL),
                             df_useNbrs(false),
                             df_useIsotopes(true), df_useChirality(true), df_useChiralityRings(true) {
      };
      AtomCompareFunctor(Canon::canon_atom *atoms, const ROMol &m,
                         const boost::dynamic_bitset<> *atomsInPlay=NULL,
                         const boost::dynamic_bitset<> *bondsInPlay=NULL ) :
        dp_atoms(atoms), dp_mol(&m),
        dp_atomsInPlay(atomsInPlay),dp_bondsInPlay(bondsInPlay),
        df_useNbrs(false),df_useIsotopes(true), df_useChirality(true), df_useChiralityRings(true) {
      };
      int operator()(int i,int j) const {
        PRECONDITION(dp_atoms,"no atoms");
        PRECONDITION(dp_mol,"no molecule");
        PRECONDITION(i!=j,"bad call");
        if(dp_atomsInPlay && !((*dp_atomsInPlay)[i] || (*dp_atomsInPlay)[j])){
          return 0;
        }

        int v=basecomp(i,j,1);
        if(v) return v;

        if(df_useNbrs){
          getAtomNeighborhood(i,dp_atoms[i].bonds);
          getAtomNeighborhood(j,dp_atoms[j].bonds);
          for(unsigned int ii=0;ii<dp_atoms[i].bonds.size() && ii<dp_atoms[j].bonds.size();++ii){
            int cmp=bondholder::compare(dp_atoms[i].bonds[ii],dp_atoms[j].bonds[ii]);
            if(cmp) return cmp;
          }

          if(dp_atoms[i].bonds.size()<dp_atoms[j].bonds.size()){
            return -1;
          } else if(dp_atoms[i].bonds.size()>dp_atoms[j].bonds.size()) {
            return 1;
          }
        }
        return 0;
      }
    };

    const unsigned int ATNUM_CLASS_OFFSET=10000;
    class ChiralAtomCompareFunctor {
      void getAtomNeighborhood(std::vector<bondholder> &nbrs) const{
        for(unsigned j=0; j < nbrs.size(); ++j){
          unsigned int nbrIdx = nbrs.at(j).nbrIdx;
          if(nbrIdx == ATNUM_CLASS_OFFSET){
            // Ignore the Hs
            continue;
          }
          const Atom *nbr = dp_atoms[nbrIdx].atom;
          nbrs.at(j).nbrSymClass = nbr->getAtomicNum()*ATNUM_CLASS_OFFSET+dp_atoms[nbrIdx].index+1;
        }
        std::sort(nbrs.begin(),nbrs.end(),bondholder::greater);
        // FIX: don't want to be doing this long-term
      }

      int basecomp(int i,int j) const {
        PRECONDITION(dp_atoms,"no atoms");
        unsigned int ivi,ivj;

        // always start with the current class:
        ivi= dp_atoms[i].index;
        ivj= dp_atoms[j].index;
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // move onto atomic number
        ivi= dp_atoms[i].atom->getAtomicNum();
        ivj= dp_atoms[j].atom->getAtomicNum();
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // isotopes:
        ivi=dp_atoms[i].atom->getIsotope();
        ivj=dp_atoms[j].atom->getIsotope();
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // atom stereochem:
        ivi=0;
        ivj=0;
        std::string cipCode;
        if(dp_atoms[i].atom->getPropIfPresent(common_properties::_CIPCode,cipCode)){
          ivi=cipCode=="R"?2:1;
        }
        if(dp_atoms[j].atom->getPropIfPresent(common_properties::_CIPCode,cipCode)){
          ivj=cipCode=="R"?2:1;
        }
        if(ivi<ivj)
          return -1;
        else if(ivi>ivj)
          return 1;

        // bond stereo is taken care of in the neighborhood comparison
        return 0;
      }
    public:
      Canon::canon_atom *dp_atoms;
      const ROMol *dp_mol;
      bool df_useNbrs;
      ChiralAtomCompareFunctor() : dp_atoms(NULL), dp_mol(NULL), df_useNbrs(false) {
      };
      ChiralAtomCompareFunctor(Canon::canon_atom *atoms, const ROMol &m) : dp_atoms(atoms), dp_mol(&m), df_useNbrs(false) {
      };
      int operator()(int i,int j) const {
        PRECONDITION(dp_atoms,"no atoms");
        PRECONDITION(dp_mol,"no molecule");
        PRECONDITION(i!=j,"bad call");
        int v=basecomp(i,j);
        if(v) return v;

        if(df_useNbrs){
          getAtomNeighborhood(dp_atoms[i].bonds);
          getAtomNeighborhood(dp_atoms[j].bonds);

          // we do two passes through the neighbor lists. The first just uses the
          // atomic numbers (by passing the optional 10000 to bondholder::compare),
          // the second takes the already-computed index into account
          for(unsigned int ii=0;ii<dp_atoms[i].bonds.size() && ii<dp_atoms[j].bonds.size();++ii){
            int cmp=bondholder::compare(dp_atoms[i].bonds[ii],dp_atoms[j].bonds[ii],ATNUM_CLASS_OFFSET);
            if(cmp) return cmp;
          }
          for(unsigned int ii=0;ii<dp_atoms[i].bonds.size() && ii<dp_atoms[j].bonds.size();++ii){
            int cmp=bondholder::compare(dp_atoms[i].bonds[ii],dp_atoms[j].bonds[ii]);
            if(cmp) return cmp;
          }
          if(dp_atoms[i].bonds.size()<dp_atoms[j].bonds.size()){
            return -1;
          } else if(dp_atoms[i].bonds.size()>dp_atoms[j].bonds.size()) {
            return 1;
          }
        }
        return 0;
      }
    };



    template <typename CompareFunc>
    void RefinePartitions(const ROMol &mol,
                          canon_atom *atoms,
                          CompareFunc compar, int mode,
                          int *order,
                          int *count,
                          int &activeset, int *next,
                          int *changed,
                          char *touchedPartitions){
      unsigned int nAtoms=mol.getNumAtoms();
      register int partition;
      register int symclass;
      register int *start;
      register int offset;
      register int index;
      register int len;
      register int i;
      //std::vector<char> touchedPartitions(mol.getNumAtoms(),0);
      
      // std::cerr<<"&&&&&&&&&&&&&&&& RP"<<std::endl;
      while( activeset != -1 ) {
        //std::cerr<<"ITER: "<<activeset<<" next: "<<next[activeset]<<std::endl;
        // std::cerr<<" next: ";
        // for(unsigned int ii=0;ii<nAtoms;++ii){
        //   std::cerr<<ii<<":"<<next[ii]<<" ";
        // }
        // std::cerr<<std::endl; 
        // for(unsigned int ii=0;ii<nAtoms;++ii){
        //   std::cerr<<order[ii]<<" count: "<<count[order[ii]]<<" index: "<<atoms[order[ii]].index<<std::endl;
        // }

        partition = activeset;
        activeset = next[partition];
        next[partition] = -2;

        len = count[partition]; 
        offset = atoms[partition].index;
        start = order+offset;
        // std::cerr<<"\n\n**************************************************************"<<std::endl;
        // std::cerr<<"  sort - class:"<<atoms[partition].index<<" len: "<<len<<":";
        // for(unsigned int ii=0;ii<len;++ii){
        //   std::cerr<<" "<<order[offset+ii]+1;
        // }
        // std::cerr<<std::endl;
        // for(unsigned int ii=0;ii<nAtoms;++ii){
        //   std::cerr<<order[ii]+1<<" count: "<<count[order[ii]]<<" index: "<<atoms[order[ii]].index<<std::endl;
        // }
        hanoisort(start,len,count,changed, compar);
        // std::cerr<<"*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*"<<std::endl;
        // std::cerr<<"  result:";
        // for(unsigned int ii=0;ii<nAtoms;++ii){
        //    std::cerr<<order[ii]+1<<" count: "<<count[order[ii]]<<" index: "<<atoms[order[ii]].index<<std::endl;
        //  }
        for(unsigned k = 0; k < len; ++k){
          changed[start[k]]=0;
        }

        index = start[0];
        // std::cerr<<"  len:"<<len<<" index:"<<index<<" count:"<<count[index]<<std::endl;
        for( i=count[index]; i<len; i++ ) {
          index = start[i];
          if( count[index] )
            symclass = offset+i;
          atoms[index].index = symclass;
          // std::cerr<<" "<<index+1<<"("<<symclass<<")";
          //if(mode && (activeset<0 || count[index]>count[activeset]) ){
          //  activeset=index;
          //}
          for(unsigned j = 0; j < atoms[index].degree; ++j){
            changed[atoms[index].nbrIds[j]]=1;
          }
        }
        // std::cerr<<std::endl;

        if( mode ) {
          index=start[0];
          for( i=count[index]; i<len; i++ ) {
            index=start[i];
            for(unsigned j = 0; j < atoms[index].degree; ++j){
              unsigned int nbor = atoms[index].nbrIds[j];
              touchedPartitions[atoms[nbor].index]=1;
            }
          }
          for(unsigned int ii=0; ii<nAtoms; ++ii) {
            if(touchedPartitions[ii]){
              partition = order[ii];
              if( (count[partition]>1) &&
                  (next[partition]==-2) ) {
                next[partition] = activeset;
                activeset = partition;
              }
              touchedPartitions[ii] = 0;
            }
          }
        }
      }
    } // end of RefinePartitions()

    template <typename CompareFunc>
    void BreakTies(const ROMol &mol,
                   canon_atom *atoms,
                   CompareFunc compar, int mode,
                   int *order,
                   int *count,
                   int &activeset, int *next,
                   int *changed,
                   char *touchedPartitions){
      unsigned int nAtoms=mol.getNumAtoms();
      register int partition;
      register int offset;
      register int index;
      register int len;
      int oldPart = 0;

      for(unsigned int i=0; i<nAtoms; i++ ) {
        partition = order[i];
        oldPart = atoms[partition].index;
        while( count[partition] > 1 ) {
          len = count[partition];
          offset = atoms[partition].index+len-1;
          index = order[offset];
          atoms[index].index = offset;
          count[partition] = len-1;
          count[index] = 1;

          for(unsigned j = 0; j < atoms[index].degree; ++j){
            unsigned int nbor = atoms[index].nbrIds[j];
            touchedPartitions[atoms[nbor].index]=1;
            changed[nbor]=1;
          }

          for(unsigned int ii=0; ii<nAtoms; ++ii) {
            if(touchedPartitions[ii]){
              int npart = order[ii];
              if( (count[npart]>1) &&
                  (next[npart]==-2) ) {
                next[npart] = activeset;
                activeset = npart;
              }
              touchedPartitions[ii] = 0;
            }
          }
          RefinePartitions(mol,atoms,compar,mode,order,count,activeset,next,changed,touchedPartitions);
        }
        //not sure if this works each time
        if(atoms[partition].index != oldPart){
          i-=1;
        }
      }
    } // end of BreakTies()

    void rankMolAtoms(const ROMol &mol,std::vector<unsigned int> &res,
                      bool breakTies=true,
                      bool includeChirality=true,
                      bool includeIsotopes=true);
    void rankFragmentAtoms(const ROMol &mol,std::vector<unsigned int> &res, 
                           const boost::dynamic_bitset<> &atomsInPlay,
                           const boost::dynamic_bitset<> &bondsInPlay,
                           const std::vector<std::string> *atomSymbols=NULL,
                           bool breakTies=true,
                           bool includeChirality=true,
                           bool includeIsotopes=true);
    void chiralRankMolAtoms(const ROMol &mol,std::vector<unsigned int> &res);
    void initCanonAtoms(const ROMol &mol,std::vector<Canon::canon_atom> &atoms,
                        bool includeChirality=true);


  } // end of Canon namespace
} // end of RDKit namespace
