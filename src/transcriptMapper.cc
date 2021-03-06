#include "transcriptMapper.hh"
#include "featureMapper.hh"
#include "feature.hh"
#include "jkinclude.hh"
#include "pslOps.hh"
#include "pslMapping.hh"
#include "frame.hh"
#include "featureTransMap.hh"
#include "typeOps.hh"
#include "annotationSet.hh"
#include <stdexcept>
#include <iostream>

/* get exon features */
FeatureVector TranscriptMapper::getExons(const Feature* transcript) {
    FeatureVector exons;
    transcript->getMatching(exons, [](const Feature* f) {
            return f->getType() == GxfFeature::EXON;
        });
    return exons;
}

/* build transcript exons PSL to query and mapping to target genome.
 * Return NULL if no mappings for whatever reason.*/
PslMapping* TranscriptMapper::allExonsTransMap(const Feature* transcript) const {
    const string& qName(transcript->getAttr(GxfFeature::TRANSCRIPT_ID_ATTR)->getVal());
    FeatureVector exons = getExons(transcript);
    // get alignment of exons to srcGenome and to targetGenome
    PslMapping* exonsMapping = FeatureTransMap(fGenomeTransMap).mapFeatures(qName, exons);
    if (exonsMapping == NULL) {
        return NULL;  // source sequence not in map
    }
    // resort using more evidence
    exonsMapping->sortMappedPsls(fTargetTranscript, fTargetGene);
    if (debug) {
        exonsMapping->dump(cerr, "Transcript Exons:", "    ");
    }
    if (not exonsMapping->haveMappings()) {
        delete exonsMapping;
        return NULL;
    } else {
        return exonsMapping;
    }
}

/* create transMap objects used to do two level mapping via exons. */
const TransMapVector TranscriptMapper::makeViaExonsTransMap(const PslMapping* exonsMapping) {
    TransMapVector transMaps;
    transMaps.push_back(TransMap::factoryFromPsls(exonsMapping->getSrcPsl(), true)); // swap map genomeA to exons
    transMaps.push_back(TransMap::factoryFromPsls(exonsMapping->getMappedPsl(), false)); // exons to genomeB
    return transMaps;
}

/* get PSL of feature mapping */
PslMapping* TranscriptMapper::featurePslMap(const Feature* feature) {
    const AttrVal* idAttr = feature->findAttr(Feature::ID_ATTR);
    const string& featureId = (idAttr != NULL) ? idAttr->getVal() : "someFeature";
    return fViaExonsFeatureTransMap->mapFeature(featureId, feature);
}

/* map one feature, linking in a child feature. */
TransMappedFeature TranscriptMapper::mapFeature(const Feature* feature) {
    PslMapping* pslMapping = (fViaExonsFeatureTransMap != NULL) ? featurePslMap(feature) : NULL;
    TransMappedFeature transMappedFeature = FeatureMapper::map(feature, pslMapping);
    transMappedFeature.setRemapStatus(fSrcSeqInMapping);
    delete pslMapping;
    return transMappedFeature;
}

/* recursive map features below transcript */
TransMappedFeature TranscriptMapper::mapFeatures(const Feature* feature) {
    TransMappedFeature transMappedFeature = mapFeature(feature);
    for (int iChild = 0; iChild < feature->getChildren().size(); iChild++) {
        TransMappedFeature childFeatures = mapFeatures(feature->getChild(iChild));
        FeatureMapper::updateParents(transMappedFeature, childFeatures);
    }
    return transMappedFeature;
}

/* create a new transcript record that covers the alignment */
ResultFeatures TranscriptMapper::mapTranscriptFeature(const Feature* transcript) {
    ResultFeatures mappedTranscript(transcript);
    struct psl* mappedPsl = (fExonsMapping != NULL) ? fExonsMapping->getMappedPsl() : NULL;
    if (mappedPsl != NULL) {
        // transcript for mapped PSLs
        mappedTranscript.mapped
            = FeatureMapper::mapBounding(transcript,
                                         string(mappedPsl->tName),
                                         mappedPsl->tStart, mappedPsl->tEnd,
                                         charToString(pslTStrand(mappedPsl)));
        mappedTranscript.mapped->setNumMappings(fExonsMapping->getMappedPsls().size());
    }

    // if any parts was unmapped, also need a copy of the original transcript
    if ((mappedPsl == NULL) or (not pslQueryFullyMapped(mappedPsl))) {
        mappedTranscript.unmapped = FeatureMapper::mapBounding(transcript);
    }
    return mappedTranscript;
}

/* constructor, targetAnnotations can be NULL */
TranscriptMapper::TranscriptMapper(const TransMap* genomeTransMap,
                                   const Feature* transcript,
                                   const AnnotationSet* targetAnnotations,
                                   bool srcSeqInMapping,
                                   ostream* transcriptPslFh):
    fGenomeTransMap(genomeTransMap), 
    fSrcSeqInMapping(srcSeqInMapping),
    fExonsMapping(NULL),
    fViaExonsFeatureTransMap(NULL),
    fTargetGene(NULL),
    fTargetTranscript(NULL) {
    assert(transcript->getType() == GxfFeature::TRANSCRIPT);

    // if available, find target transcripts to use in selecting multiple mappings.  Special handling
    // for PAR requires sequence id.
    if (targetAnnotations != NULL) {
        fTargetGene = targetAnnotations->getFeatureById(transcript->getAttrValue(GxfFeature::GENE_ID_ATTR),
                                                        transcript->getSeqid());
        fTargetTranscript = targetAnnotations->getFeatureById(transcript->getAttrValue(GxfFeature::TRANSCRIPT_ID_ATTR),
                                                              transcript->getSeqid());
    }

    // map all exons together, this will be used to project the other exons
    fExonsMapping = allExonsTransMap(transcript);
    if (fExonsMapping != NULL) {
        if (transcriptPslFh != NULL) {
            fExonsMapping->writeMapped(*transcriptPslFh);
        }
        fVaiExonsTransMaps = makeViaExonsTransMap(fExonsMapping);
        fViaExonsFeatureTransMap = new FeatureTransMap(fVaiExonsTransMaps);
    }
}

/* destructor */
TranscriptMapper::~TranscriptMapper() {
    delete fExonsMapping;
    fVaiExonsTransMaps.free();
    delete fViaExonsFeatureTransMap;
}

/*
 * map one transcript's annotations.  Fill in transcript
 */
ResultFeatures TranscriptMapper::mapTranscriptFeatures(const Feature* transcript) {
    // project features via exons (including redoing exons)
    ResultFeatures mappedTranscript = mapTranscriptFeature(transcript);
    TransMappedFeature mappedTranscriptSet(mappedTranscript);
    for (int iChild = 0; iChild < transcript->getChildren().size(); iChild++) {
        TransMappedFeature transMappedFeature = mapFeatures(transcript->getChild(iChild));
        FeatureMapper::updateParents(mappedTranscriptSet, transMappedFeature);
    }

    mappedTranscript.setBoundingFeatureRemapStatus(fSrcSeqInMapping);
    mappedTranscript.setNumMappingsAttr();
    return mappedTranscript;
}
