#ifndef _ALLJOYN_PERMISSION_MGMT_OBJ_H
#define _ALLJOYN_PERMISSION_MGMT_OBJ_H
/**
 * @file
 * This file defines the Permission DB classes that provide the interface to
 * parse the authorization data
 */

/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#ifndef __cplusplus
#error Only include PermissionMgmtObj.h in C++ code.
#endif

#include <memory>

#include <alljoyn/BusObject.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/GUID.h>
#include <qcc/KeyBlob.h>
#include <qcc/CryptoECC.h>
#include <qcc/CertificateECC.h>
#include <qcc/LockLevel.h>
#include <alljoyn/PermissionPolicy.h>
#include "CredentialAccessor.h"
#include "ProtectedAuthListener.h"
#include "PeerState.h"
#include "KeyStore.h"
#include "BusUtil.h"

namespace ajn {

class MessageEncryptionNotification {
  public:
    MessageEncryptionNotification()
    {
    }
    virtual ~MessageEncryptionNotification()
    {
    }

    /**
     * Notify the observer when the message encryption step is complete.
     */
    virtual void EncryptionComplete()
    {
    }
};

class PermissionMgmtObj : public BusObject {

    friend class PermissionManager;

  public:

    /**
     * Error name Permission Denied.  Error raised when the message is not
     * authorized.
     */
    static const char* ERROR_PERMISSION_DENIED;

    /**
     * Error name Invalid Certificate.  Error raised when the certificate or
     * cerficate chain is not valid.
     */
    static const char* ERROR_INVALID_CERTIFICATE;

    /**
     * Error name Invalid certificate usage.  Error raised when the extended
     * key usage (EKU) is not AllJoyn specific.
     */
    static const char* ERROR_INVALID_CERTIFICATE_USAGE;

    /**
     * Error name Digest Mismatch.  Error raised when the digest of the
     * manifest does not match the digest listed in the identity certificate.
     */
    static const char* ERROR_DIGEST_MISMATCH;

    /**
     * Error name Policy Not Newer.  Error raised when the new policy does not
     * have a greater version number than the existing policy.
     */
    static const char* ERROR_POLICY_NOT_NEWER;

    /**
     * Error name Duplicate Certificate.  Error raised when the certificate
     * is already installed.
     */
    static const char* ERROR_DUPLICATE_CERTIFICATE;

    /**
     * Error name Certificate Not Found.  Error raised when the certificate
     * is not found.
     */
    static const char* ERROR_CERTIFICATE_NOT_FOUND;

    /**
     * Error name Management Already Started.  Error raised when the app being
     * managed detects that Security Manager called StartManagement two times,
     * without calling EndManagement in between these two calls. This error
     * typically means that the first management session has been interrupted
     * abruptly - e.g. by losing network connectivity between the Security Manager
     * and the app being managed during the management session.
     */
    static const char* ERROR_MANAGEMENT_ALREADY_STARTED;

    /**
     * Error name Management Not Started.  Error raised when the app being
     * managed detects that Security Manager called EndManagement without
     * a matching StartManagement. This error typically means that the previous
     * management session has been interrupted abruptly - e.g. by restarting
     * the app being managed during the management session.
     */
    static const char* ERROR_MANAGEMENT_NOT_STARTED;

    /**
     * For the SendMemberships call, the app sends one cert chain at time since
     * thin client peer may not be able to handle large amount of data.  The app
     * reads back the membership cert chain from the peer.  It keeps looping until
     * both sides exchanged all the relevant membership cert chains.
     * A send code of
     *     SEND_MEMBERSHIP_NONE indicates the peer does not have any membership
     *          cert chain or already sent all of its membership cert chain in
     *          previous replies.
     */
    static const uint8_t SEND_MEMBERSHIP_NONE = 0;
    /**
     *     SEND_MEMBERSHIP_MORE indicates the peer will send more membership
     *          cert chains.
     */
    static const uint8_t SEND_MEMBERSHIP_MORE = 1;
    /**
     *     SEND_MEMBERSHIP_LAST indicates the peer sends the last membership
     *          cert chain.
     */
    static const uint8_t SEND_MEMBERSHIP_LAST = 2;

    class KeyExchangeListener : public ProtectedAuthListener {
      public:
        KeyExchangeListener()
        {
        }
        void SetPermissionMgmtObj(PermissionMgmtObj* pmo)
        {
            this->pmo = pmo;
        }

        /**
         * Simply wraps the call of the same name to the inner AuthListener
         */
        bool RequestCredentials(const char* authMechanism, const char* peerName, uint16_t authCount, const char* userName, uint16_t credMask, Credentials& credentials);

        /**
         * Simply wraps the call of the same name to the inner ProtectedAuthListener
         */
        bool VerifyCredentials(const char* authMechanism, const char* peerName, const Credentials& credentials);

      private:
        PermissionMgmtObj* pmo;
    };

    typedef enum {
        TRUST_ANCHOR_CA = 0,            ///< certificate authority
        TRUST_ANCHOR_SG_AUTHORITY = 1,  ///< security group authority
    } TrustAnchorType;

    struct TrustAnchor {
        TrustAnchorType use;
        qcc::KeyInfoNISTP256 keyInfo;
        qcc::GUID128 securityGroupId;

        TrustAnchor() : use(TRUST_ANCHOR_CA), securityGroupId(0)
        {
        }
        TrustAnchor(TrustAnchorType use) : use(use), securityGroupId(0)
        {
        }
        TrustAnchor(TrustAnchorType use, const qcc::KeyInfoNISTP256& keyInfo) : use(use), keyInfo(keyInfo), securityGroupId(0)
        {
        }
    };

    /**
     * The list of trust anchors
     */
    class TrustAnchorList : public std::vector<std::shared_ptr<TrustAnchor> > {
      public:
        TrustAnchorList() : lock(qcc::LOCK_LEVEL_PERMISSIONMGMTOBJ_LOCK)
        {
        }

        TrustAnchorList(const TrustAnchorList& other) : std::vector<std::shared_ptr<TrustAnchor> >(other)
        {
        }

        TrustAnchorList& operator=(const TrustAnchorList& other)
        {
            std::vector<std::shared_ptr<TrustAnchor> >::operator=(other);
            return *this;
        }

        QStatus Lock()
        {
            return lock.Lock();
        }

        QStatus Lock(const char* file, uint32_t line)
        {
            return lock.Lock(file, line);
        }

        QStatus Unlock()
        {
            return lock.Unlock();
        }

        QStatus Unlock(const char* file, uint32_t line)
        {
            return lock.Unlock(file, line);
        }

      private:
        /* Use a member variable instead of inheriting from qcc::Mutex because copying qcc::Mutex objects is not supported */
        qcc::Mutex lock;
    };

    /**
     * Constructor
     *
     * Must call Init() before using this BusObject.
     */
    PermissionMgmtObj(BusAttachment& bus, const char* objectPath);

    /**
     * virtual destructor
     */
    virtual ~PermissionMgmtObj();

    /**
     * Initialize and Register this BusObject with to the BusAttachment.
     *
     * @return
     *  - #ER_OK on success
     *  - An error status otherwise
     */
    virtual QStatus Init();

    /**
     * Generates the message args to send the membership data to the peer.
     * @param args[out] the vector of the membership cert chain args.
     * @param remotePeerGuid[in] the peer's authentication GUID.
     * @return
     *         - ER_OK if successful.
     *         - an error status otherwise.
     */
    QStatus GenerateSendMemberships(std::vector<std::vector<MsgArg*> >& args, const qcc::GUID128& remotePeerGuid);

    /**
     * Parse the message received from the PermissionMgmt's SendMembership method.
     * @param msg the message
     * @param[in,out] done output flag to indicate that process is complete
     * @return
     *         - ER_OK if successful.
     *         - an error status otherwise.
     */
    QStatus ParseSendMemberships(Message& msg, bool& done);
    QStatus ParseSendMemberships(Message& msg)
    {
        bool done = false;
        return ParseSendMemberships(msg, done);
    }

    /**
     * Parse the message received from the org.alljoyn.bus.Peer.Authentication's
     * SendManifests method.
     * @param msg the message
     * @param peerState the peer state
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus ParseSendManifests(Message& msg, PeerState& peerState);

    /**
     * Is there any trust anchor installed?
     * @return true if there is at least one trust anchors installed; false, otherwise.
     */
    bool HasTrustAnchors();

    /**
     * Retrieve the list of trust anchors.
     * @return the list of trust anchors
     */
    const TrustAnchorList& GetTrustAnchors()
    {
        return trustAnchors;
    }

    /**
     * Help function to store DSA keys in the key store.
     * @param ca the credential accesor object
     * @param privateKey the DSA private key
     * @param publicKey the DSA public key
     * @return ER_OK if successful; otherwise, error code.
     */
    static QStatus StoreDSAKeys(CredentialAccessor* ca, const qcc::ECCPrivateKey* privateKey, const qcc::ECCPublicKey* publicKey);

    /**
     * Set the permission manifest template for the application.
     * @params rules the permission rules.
     * @params count the number of permission rules
     */
    QStatus SetManifestTemplate(const PermissionPolicy::Rule* rules, size_t count);

    /**
     * Retrieve the claimable state of the application.
     * @return the claimable state
     */
    PermissionConfigurator::ApplicationState GetApplicationState() const;

    /**
     * Set the application state.  The state can't be changed from CLAIMED to
     * CLAIMABLE.
     * @param newState The new application state
     * @return
     *      - #ER_OK if action is allowed.
     *      - #ER_INVALID_APPLICATION_STATE if the state can't be changed
     *      - #ER_NOT_IMPLEMENTED if the method is not implemented
     */
    QStatus SetApplicationState(PermissionConfigurator::ApplicationState state);

    /**
     * Reset the Permission module by removing all the trust anchors, DSA keys,
     * installed policy and certificates.
     * @return ER_OK if successful; otherwise, an error code.
     */
    QStatus Reset();

    /**
     * Get the connected peer authentication metadata.
     * @param guid the peer guid
     * @param[out] authMechanism the authentication mechanism (ie the key exchange name)
     * @param[out] publicKeyFound the flag indicating whether the peer has an ECC public key
     * @param[out] publicKey the buffer to hold the ECC public key.  Pass NULL
     *                       to skip.
     * @param[out] identityCertificateThumbprint buffer to receive the SHA-256 thumbprint of the identity certificate
     * @param[out] issuerPublicKeys the vector for the list of issuer public
     *                               keys.
     * @return ER_OK if successful; otherwise, error code.
     */
    QStatus GetConnectedPeerAuthMetadata(const qcc::GUID128& guid, qcc::String& authMechanism, bool& publicKeyFound, qcc::ECCPublicKey* publicKey, uint8_t* identityCertificateThumbprint, std::vector<qcc::ECCPublicKey>& issuerPublicKeys);

    /**
     * Get the connected peer ECC public key if the connection uses the
     * ECDHE_ECDSA key exchange.
     * @param guid the peer guid
     * @param[out] publicKey the buffer to hold the ECC public key.  Pass NULL
     *                       to skip.
     * @param[out] issuerPublicKeys the vector for the list of issuer public
     *                               keys.
     * @return ER_OK if successful; otherwise, error code.
     */
    QStatus GetConnectedPeerPublicKey(const qcc::GUID128& guid, qcc::ECCPublicKey* publicKey, std::vector<qcc::ECCPublicKey>& issuerPublicKeys);

    /**
     * Get the connected peer ECC public key if the connection uses the
     * ECDHE_ECDSA key exchange.
     * @param guid the peer guid
     * @param[out] publicKey the buffer to hold the ECC public key.
     */
    QStatus GetConnectedPeerPublicKey(const qcc::GUID128& guid, qcc::ECCPublicKey* publicKey);

    /**
     * Retrieve the membership certificate map.
     * @return the membership certificate map.
     */
    _PeerState::GuildMap& GetGuildMap()
    {
        return guildMap;
    }

    /**
     * Load the internal data from the key store
     */
    void Load();

    QStatus InstallTrustAnchor(TrustAnchor* trustAnchor);
    QStatus StoreIdentityCertChain(size_t certChainCount, const qcc::CertificateX509* certs);
    QStatus RetrievePolicy(PermissionPolicy& policy, bool defaultPolicy = false);
    QStatus StorePolicy(PermissionPolicy& policy, bool defaultPolicy = false);
    QStatus StoreMembership(const MsgArg& certArg);
    QStatus StoreManifests(size_t manifestCount, const Manifest* signedManifests, bool append);
    QStatus GetMembershipSummaries(MsgArg& arg);

    /**
     * Retrieve certificates in a MsgArg used to transmit certificates in the standard format,
     * used by Claim and GetIdentity.
     *
     * @param[in] certArg MsgArg containing the wire-format certificates
     * @param[out] certs Vector to receive the certificates
     *
     * @return
     *    - #ER_OK if certificates are successfully retrieved into certs
     *    - other error indicating failure
     */
    QStatus RetrieveCertsFromMsgArg(const MsgArg& certArg, std::vector<qcc::CertificateX509>& certs);

    /**
     * Retrieve manifests in a MsgArg used to transmit manifests in the standard format, as used
     * by Claim and InstallManifests.
     *
     * @param[in] signedManifestsArg MsgArg containing the wire-format signed manifests
     * @param[out] manifests Vector to receive the manifests
     *
     * @return
     *    - #ER_OK if manifests are successfully retrieved into manifests
     *    - other error code indicating failure
     */
    QStatus RetrieveManifestsFromMsgArg(const MsgArg& signedManifestsArg, std::vector<Manifest>& manifests);


    /**
     * Generate the SHA-256 digest for the manifest data.
     * @param bus the bus attachment
     * @param rules the rules for the manifest
     * @param count the number of rules
     * @param[out] digest the buffer to store the digest
     * @param[out] digestSize the size of the buffer.  Expected to be Crypto_SHA256::DIGEST_SIZE
     * @return ER_OK for success; error code otherwise.
     */
    static QStatus GenerateManifestDigest(BusAttachment& bus, const PermissionPolicy::Rule* rules, size_t count, uint8_t* digest, size_t digestSize);

    /**
     * Retrieve the manifest from persistence store.
     * @param[out] manifests The variable to hold the manifests.
     * @return ER_OK for success; error code otherwise.
     */
    QStatus RetrieveManifests(std::vector<Manifest>& manifests);
    /**
     * Reply to a method call with an error message.
     *
     * @param msg        The method call message
     * @param status     The status code for the error
     * @return
     *      - #ER_OK if successful
     *      - #ER_BUS_OBJECT_NOT_REGISTERED if bus object has not yet been registered
     *      - An error status otherwise
     */
    QStatus MethodReply(const Message& msg, QStatus status);

    /**
     * The State signal is used to advertise the state of an application.  It is
     * sessionless, because the signal is intended to discover applications.
     * Discovery is not done by using 'About'.  Applications must add extra code
     * to provide About.
     *
     * Not all applications will do this as pure consumer applications don't
     * need to be discovered by other applications.  Still they need to be
     * discovered by the framework to support certain some core framework
     * features. Furthermore we want to avoid interference between core
     * framework events and application events.
     *
     * The application state is an enumeration representing the current state of
     * the application.
     *
     * The list of valid values:
     * | Value | Description                                                       |
     * |-------|-------------------------------------------------------------------|
     * | 0     | NotClaimable.  The application is not claimed and not accepting   |
     * |       | claim requests.                                                   |
     * | 1     | Claimable.  The application is not claimed and is accepting claim |
     * |       | requests.                                                         |
     * | 2     | Claimed. The application is claimed and can be configured.        |
     * | 3     | NeedUpdate. The application is claimed, but requires a            |
     * |       | configuration update (after a software upgrade).                  |
     *
     * @param[in] publicKeyInfo the application public key
     * @param[in] state the application state
     *
     * @return
     *   - #ER_OK on success
     *   - An error status otherwise.
     */
    virtual QStatus State(const qcc::KeyInfoNISTP256& publicKeyInfo, PermissionConfigurator::ApplicationState state) = 0;

    /**
     * Set the authentication mechanisms the application supports for the
     * claim process.  It is a bit mask.
     *
     * | Mask  | Description                                                   |
     * |-------|---------------------------------------------------------------|
     * | 0x1   | claiming via ECDHE_NULL                                       |
     * | 0x2   | claiming via ECDHE_PSK                                        |
     * | 0x4   | claiming via ECDHE_ECDSA                                      |
     *
     * @param[in] claimCapabilities The authentication mechanisms the application supports
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus SetClaimCapabilities(PermissionConfigurator::ClaimCapabilities claimCapabilities);

    /**
     * Set the additional information on the claim capabilities.
     * It is a bit mask.
     *
     * | Mask  | Description                                                   |
     * |-------|---------------------------------------------------------------|
     * | 0x1   | PSK generated by Security Manager                             |
     * | 0x2   | PSK generated by application                                  |
     *
     * @param[in] additionalInfo The additional info
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus SetClaimCapabilityAdditionalInfo(PermissionConfigurator::ClaimCapabilityAdditionalInfo& additionalInfo);

    /**
     * Get the authentication mechanisms the application supports for the
     * claim process.
     *
     * @param[out] claimCapabilities The authentication mechanisms the application supports
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus GetClaimCapabilities(PermissionConfigurator::ClaimCapabilities& claimCapabilities) const;


    /**
     * Get the additional information on the claim capabilities.
     * @param[out] additionalInfo The additional info
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus GetClaimCapabilityAdditionalInfo(PermissionConfigurator::ClaimCapabilityAdditionalInfo& additionalInfo) const;

    /**
     * Store a membership certificate chain.
     * @param certChain the array of CertificateX509 objects in the chain.
     * @param count the size of the certificate chain.
     *
     * @return
     *  - #ER_OK if successful
     *  - an error status indicating failure
     */
    QStatus StoreMembership(const qcc::CertificateX509* certChain, size_t count);
    /**
     * Get the ECC public key from the keystore.
     * @param[out] publicKeyInfo the public key
     * @return ER_OK if successful; otherwise, error code.
     */
    QStatus GetPublicKey(qcc::KeyInfoNISTP256& publicKeyInfo);

    /**
     * Is ready for service?
     * @see Load()
     * @return true if it is ready for service; false, otherwise.
     */
    bool IsReady()
    {
        return ready;
    }

    /**
     * Send any needed manifests in advance of a message about to be sent.
     *
     * @param[in] remotePeerObj The ProxyBusObject being asked to send the message, and that we can use to send manifests.
     * @param[in] msg The message about to be sent. If nullptr, all manifests will be sent.
     *
     * @remarks remotePeerObj and msg cannot both be nullptr.
     *
     * @return #ER_OK if all needed manifests were sent, possibly none.
     * @return #ER_BAD_ARG_1 if both remotePeerObj and msgPtr are nullptr
     *         - other error indicating failure
     */
    QStatus SendManifests(const ProxyBusObject* remotePeerObj, Message* msg);

    /**
     * Perform claiming of this app locally/offline.
     *
     * @param[in] certificateAuthority Certificate authority trust anchor information
     * @param[in] adminGroupAuthority Admin group authority trust anchor information
     * @param[in] certs Identity cert chain
     * @param[in] certCount Count of certs array
     * @param[in] manifests Signed manifests
     * @param[in] manifestCount Count of manifests array
     *
     * @return
     *   - #ER_OK if the app was successfully claimed
     *   - #ER_FAIL if the app could not be claimed, but could not then be reset back to original state.
     *              App will be in unknown state in this case.
     *   - other error code indicating failure, but app is returned to reset state
     */
    QStatus Claim(
        const TrustAnchor& certificateAuthority,
        const TrustAnchor& adminGroupAuthority,
        const qcc::CertificateX509* certs,
        size_t certCount,
        const Manifest* manifests,
        size_t manifestCount);

    /**
     * Perform a local UpdateIdentity.
     *
     * @param[in] certs Identity cert chain to be installed
     * @param[in] certCount Number of certificates in certs array
     * @param[in] manifests Signed manifests to be installed
     * @param[in] manifestCount Number of signed manifests in manifests array
     *
     * @return
     *    - #ER_OK if the identity was successfully updated
     *    - other error code indicating failure
     */
    QStatus UpdateIdentity(
        const qcc::CertificateX509* certs,
        size_t certCount,
        const Manifest* manifests,
        size_t manifestCount);

    /**
     * Retrieve the local app's identity certificate chain.
     *
     * @param[out] certChain A vector containing the identity certificate chain
     *
     * @return
     *    - #ER_OK if the chain is successfully stored in certChain
     *    - other error indicating failure
     */
    QStatus GetIdentity(std::vector<qcc::CertificateX509>& certChain);

    /**
     * Reset the local app's policy.
     *
     * @return
     *    - #ER_OK if the policy was successfully reset
     *    - other error code indicating failure
     */
    QStatus ResetPolicy();

    /**
     * Remove a membership certificate.
     *
     * @param[in] serial Serial number of the certificate
     * @param[in] issuerPubKey Certificate issuer's public key
     * @param[in] issuerAki Certificate issuer's AKI
     *
     * @return
     *    - #ER_OK if the certificate was found and removed
     *    - #ER_CERTIFICATE_NOT_FOUND if the certificate was not found
     *    - other error code indicating failure
     */
    QStatus RemoveMembership(const qcc::String& serial, const qcc::ECCPublicKey* issuerPubKey, const qcc::String& issuerAki);

    /**
     * Signal the app locally that management is starting.
     *
     * @return
     *    - #ER_OK if the start management signal was sent
     *    - #ER_MANAGEMENT_ALREADY_STARTED if the app is already in this state
     */
    QStatus StartManagement();

    /**
     * Signal the app locally that management is ending.
     *
     * @return
     *    - #ER_OK if the start management signal was sent
     *    - #ER_MANAGEMENT_NOT_STARTED if the app was not in the management state
     */
    QStatus EndManagement();

    /**
     * Retrieve the IdentityCertificateId property.
     *
     * @param[out] serial Identity certificate's serial
     * @param[out] keyInfo Identity certificate's KeyInfoNISTP256 structure
     *
     * @return
     *    - #ER_OK if the identity certificate information is placed in the parameters
     *    - #ER_CERTIFICATE_NOT_FOUND if no identity certificate is installed
     *    - other error code indicating failure
     */
    QStatus RetrieveIdentityCertificateId(qcc::String& serial, qcc::KeyInfoNISTP256& issuerKeyInfo);

    /**
     * Install a new active policy.
     *
     * @param[in] policy Policy to install
     *
     * @return
     *    - #ER_OK if the policy is successfully installed
     *    - other error code indicating failure
     */
    QStatus InstallPolicy(const PermissionPolicy& policy);

    /**
     * Get the manifest template.
     *
     * @param[out] manifestTemplate Vector to receive the set of rules from the manifest template.
     *
     * @return
     *    - #ER_OK if the manifest template is successfully retrieved
     *    - other error code indicating failure
     */
    QStatus GetManifestTemplate(std::vector<PermissionPolicy::Rule>& manifestTemplate);

  protected:
    void Claim(const InterfaceDescription::Member* member, Message& msg);
    QStatus ClaimInternal(
        const TrustAnchor& certificateAuthority,
        const TrustAnchor& adminGroupAuthority,
        const qcc::CertificateX509* certs,
        size_t certCount,
        const Manifest* manifests,
        size_t manifestCount);
    QStatus RemoveMembershipInternal(const qcc::String& serial, const qcc::ECCPublicKey* issuerPubKey, const qcc::String& issuerAki);
    BusAttachment& bus;
    QStatus GetIdentity(MsgArg& arg);
    QStatus GetIdentityLeafCert(qcc::IdentityCertificate& cert);
    void Reset(const InterfaceDescription::Member* member, Message& msg);
    void InstallIdentity(const InterfaceDescription::Member* member, Message& msg);
    void InstallPolicy(const InterfaceDescription::Member* member, Message& msg);

    QStatus GetPolicy(MsgArg& msgArg);
    QStatus RebuildDefaultPolicy(PermissionPolicy& defaultPolicy);
    QStatus GetDefaultPolicy(MsgArg& msgArg);
    void ResetPolicy(const InterfaceDescription::Member* member, Message& msg);
    void InstallMembership(const InterfaceDescription::Member* member, Message& msg);
    void RemoveMembership(const InterfaceDescription::Member* member, Message& msg);
    void StartManagement(const InterfaceDescription::Member* member, Message& msg);
    void EndManagement(const InterfaceDescription::Member* member, Message& msg);
    void InstallManifests(const InterfaceDescription::Member* member, Message& msg);
    QStatus GetManifestTemplate(MsgArg& arg);
    QStatus GetManifestTemplateDigest(MsgArg& arg);
    PermissionConfigurator::ApplicationState applicationState;
    uint32_t policyVersion;
    uint16_t claimCapabilities;
    uint16_t claimCapabilityAdditionalInfo;

  private:

    typedef enum {
        ENTRY_DEFAULT_POLICY,      ///< Default policy data
        ENTRY_POLICY,              ///< Local policy data
        ENTRY_MEMBERSHIPS,         ///< the list of membership certificates and associated policies
        ENTRY_IDENTITY,            ///< the identity cert
        ENTRY_MANIFEST_TEMPLATE,   ///< The manifest template
        ENTRY_MANIFEST,            ///< The manifest data
        ENTRY_CONFIGURATION        ///< The configuration data
    } ACLEntryType;

    struct Configuration {
        uint8_t version;
        uint8_t applicationStateSet;
        uint8_t applicationState;
        uint16_t claimCapabilities;
        uint16_t claimCapabilityAdditionalInfo;

        Configuration() : version(1), applicationStateSet(0), applicationState(PermissionConfigurator::NOT_CLAIMABLE), claimCapabilities(PermissionConfigurator::CLAIM_CAPABILITIES_DEFAULT), claimCapabilityAdditionalInfo(0)
        {
        }
    };

    typedef std::map<KeyStore::Key, qcc::MembershipCertificate*> MembershipCertMap;

    class PortListener : public SessionPortListener {

      public:

        PortListener() : SessionPortListener()
        {
        }

        bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
        {
            QCC_UNUSED(joiner);
            QCC_UNUSED(opts);
            return (ALLJOYN_SESSIONPORT_PERMISSION_MGMT == sessionPort);
        }
    };

    void GetPublicKey(const InterfaceDescription::Member* member, Message& msg);
    void GetACLKey(ACLEntryType aclEntryType, KeyStore::Key& guid);
    QStatus StoreTrustAnchors();
    QStatus LoadTrustAnchors();

    QStatus StateChanged();

    QStatus GetIdentityBlob(qcc::KeyBlob& kb);
    bool ValidateCertChain(bool verifyIssuerChain, bool validateTrust, const qcc::CertificateX509* certChain, size_t count, bool enforceAKI = true);
    bool ValidateCertChainPEM(const qcc::String& certChainPEM, bool& authorized, bool enforceAKI = true);
    QStatus LocateMembershipEntry(const qcc::String& serialNum, const qcc::String& issuerAki, KeyStore::Key& membershipKey);
    void ClearMembershipCertMap(MembershipCertMap& certMap);
    QStatus GetAllMembershipCerts(MembershipCertMap& certMap, bool loadCert);
    QStatus GetAllMembershipCerts(MembershipCertMap& certMap);
    void ClearTrustAnchors();
    void PolicyChanged(PermissionPolicy* policy);
    QStatus StoreConfiguration(const Configuration& config);
    QStatus GetConfiguration(Configuration& config);
    QStatus PerformReset(bool keepForClaim);
    QStatus SameSubjectPublicKey(const qcc::CertificateX509& cert, bool& outcome);
    bool IsTrustAnchor(const qcc::ECCPublicKey* publicKey);
    QStatus ManageTrustAnchors(PermissionPolicy* policy);
    QStatus GetDSAPrivateKey(qcc::ECCPrivateKey& privateKey);
    QStatus RetrieveIdentityCertChain(MsgArg** certArgs, size_t* count);
    QStatus RetrieveIdentityCertChainPEM(qcc::String& pem);
    QStatus StoreApplicationState();
    QStatus LoadManifestTemplate(PermissionPolicy& policy);
    bool HasDefaultPolicy();
    bool IsRelevantMembershipCert(std::vector<MsgArg*>& membershipChain, std::vector<qcc::ECCPublicKey> peerIssuers);
    QStatus LookForManifestTemplate(bool& exist);

    /* Bind to an exclusive port for PermissionMgmt object */
    QStatus BindPort();

    CredentialAccessor* ca;
    TrustAnchorList trustAnchors;
    _PeerState::GuildMap guildMap;
    PortListener* portListener;
    MessageEncryptionNotification* callbackToClearSecrets;
    bool ready;

    /* Has bool semantic but using volatile int32_t for atomic compare-and-exchange */
    volatile int32_t managementStarted;
};

}
#endif
