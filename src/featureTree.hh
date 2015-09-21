/*
 * Tree structure use to store genes.
 */
#ifndef featureTree_hh
#define featureTree_hh
#include <assert.h>
#include <functional>
#include "gxf.hh"
#include "remapStatus.hh"

/* Remap status attribute name */
extern const string REMAP_STATUS_ATTR;

/* Attribute name used for original id before remap */
extern const string REMAP_ORIGINAL_ID_ATTR;

/* Attribute name used for original location before remap */
extern const string REMAP_ORIGINAL_LOCATION_ATTR;

/* Attribute name for count of mappings, set on transcripts or genes */
extern const string REMAP_NUM_MAPPINGS_ATTR;

/**
 * Tree container for a GxfFeature object and children
 */
class FeatureNode {
    public:
    GxfFeature* fFeature;
    FeatureNode* fParent;
    vector<FeatureNode*> fChildren;
    RemapStatus fRemapStatus;
    int fNumMappings;    // Number of location feature was mapped too.  Not set for all node types.
    GxfFeatureVector fMappedFeatures;
    GxfFeatureVector fUnmappedFeatures;
    GxfFeatureVector fAllOutputFeatures;   // use for debugging, as it tracks order added.

    public:
    FeatureNode(GxfFeature* feature):
        fFeature(feature),
        fParent(NULL),
        fRemapStatus(REMAP_STATUS_NONE),
        fNumMappings(0) {
    }

    ~FeatureNode() {
        delete fFeature;
        for (size_t i = 0; i < fMappedFeatures.size(); i++) {
            delete fMappedFeatures[i];
        }
        for (size_t i = 0; i < fUnmappedFeatures.size(); i++) {
            delete fUnmappedFeatures[i];
        }
        for (size_t i = 0; i < fChildren.size(); i++) {
            delete fChildren[i];
        }
    }

    /* recursively get a list features matching the specified filter */
    void getMatching(GxfFeatureVector& hits,
                     function<bool(const GxfFeature*)>(filter)) const {
        if (filter(fFeature)) {
            hits.push_back(fFeature);
        }
        for (int i = 0; i < fChildren.size(); i++) {
            fChildren[i]->getMatching(hits, filter);
        }
    }
    
    
    /* add a child node, linking up parents */
    void addChild(FeatureNode* node) {
        assert(node->fParent == NULL);
        fChildren.push_back(node);
        node->fParent = this;
    }

    /* add a mapped and take ownership */
    void addMapped(GxfFeature* mappedFeature) {
        fMappedFeatures.push_back(mappedFeature);
        fAllOutputFeatures.push_back(mappedFeature);
    }

    /* add a unmapped and take ownership */
    void addUnmapped(GxfFeature* unmappedFeature) {
        fUnmappedFeatures.push_back(unmappedFeature);
        fAllOutputFeatures.push_back(unmappedFeature);
    }

    /* compute the remap status of the feature. srcSeqInMapping
     * indicates of the srcSequence qs in the genomic map */
    RemapStatus calcRemapStatus(bool srcSeqInMapping) const;

    /* set remap status to specified value */
    void setRemapStatus(RemapStatus remapStatus) {
        fRemapStatus = remapStatus;
    }

    /* recursively determine the remap status */
    void setRemapStatusFromChildren(RemapStatus baseStatus = REMAP_STATUS_NONE);

    /* recursively determine the remap status */
    void recursiveCalcRemapStatus(bool srcSeqInMapping);

    /* print node for debugging */
    void dumpNode(ostream& fh) const;

    /* recursively print for debugging */
    void dump(ostream& fh) const;

    /* set the remap number of mappings attribute on this node  */
    void setNumMappingsAttr();

    /* recursively set the remap status attribute */
    void setRemapStatusAttr();

    /* depth-first output */
    void write(ostream& fh) const;
};

/**
 * Group genes records together in a tree.
 */
class GeneTree {
    private:
    static void queueRecords(GxfParser *gxfParser,
                             GxfRecordVector& gxfRecords);
    static FeatureNode* findGff3Parent(FeatureNode* geneTreeLeaf,
                                const GxfFeature* gxfFeature);
    static FeatureNode* loadGff3GeneRecord(GxfFeature* gxfFeature,
                                    FeatureNode* geneTreeLeaf);
    static const string& getGtfParentType(const string& featureType);
    static FeatureNode* findGtfParent(FeatureNode* geneTreeLeaf,
                                  const GxfFeature* gxfFeature);
    static FeatureNode* loadGtfGeneRecord(GxfFeature* gxfFeature,
                                   FeatureNode* geneTreeLeaf);    
    static bool loadGeneRecord(GxfParser *gxfParser,
                               GxfRecord* gxfRecord,
                               FeatureNode* geneTreeRoot,
                               FeatureNode*& geneTreeLeaf,
                               GxfRecordVector& queuedRecords);
    static FeatureNode* loadGene(GxfParser *gxfParser,
                                 GxfFeature* geneFeature);
    public:
    /* factory */
    static FeatureNode* geneTreeFactory(GxfParser *gxfParser,
                                        GxfFeature* geneFeature);
};

#endif
