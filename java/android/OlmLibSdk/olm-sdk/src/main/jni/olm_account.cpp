/*
 * Copyright 2016 OpenMarket Ltd
 * Copyright 2016 Vector Creations Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "olm_account.h"

using namespace AndroidOlmSdk;

/**
* Init memory allocation for account creation.
* @return valid memory allocation, NULL otherwise
**/
OlmAccount* initializeAccountMemory()
{
    size_t accountSize = olm_account_size();
    OlmAccount* accountPtr = (OlmAccount*)malloc(accountSize);

    if (accountPtr)
    {
        // init account object
        accountPtr = olm_account(accountPtr);
        LOGD("## initializeAccountMemory(): success - OLM account size=%lu",static_cast<long unsigned int>(accountSize));
    }
    else
    {
        LOGE("## initializeAccountMemory(): failure - OOM");
    }

    return accountPtr;
}

JNIEXPORT jlong OLM_ACCOUNT_FUNC_DEF(createNewAccountJni)(JNIEnv *env, jobject thiz)
{
    LOGD("## createNewAccountJni(): IN");
    OlmAccount* accountPtr = initializeAccountMemory();

    LOGD(" ## createNewAccountJni(): success - accountPtr=%p (jlong)(intptr_t)accountPtr=%lld",accountPtr,(jlong)(intptr_t)accountPtr);
    return (jlong)(intptr_t)accountPtr;
}

/**
 * Release the account allocation made by initializeAccountMemory().<br>
 * This method MUST be called when java counter part account instance is done.
 *
 */
JNIEXPORT void OLM_ACCOUNT_FUNC_DEF(releaseAccountJni)(JNIEnv *env, jobject thiz)
{
    LOGD("## releaseAccountJni(): IN");

    OlmAccount* accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);

    if (!accountPtr)
    {
        LOGE(" ## releaseAccountJni(): failure - invalid Account ptr=NULL");
    }
    else
    {
        LOGD(" ## releaseAccountJni(): accountPtr=%p",accountPtr);
        olm_clear_account(accountPtr);

        LOGD(" ## releaseAccountJni(): IN");
        // even if free(NULL) does not crash, logs are performed for debug purpose
        free(accountPtr);
        LOGD(" ## releaseAccountJni(): OUT");
    }
}

/**
* Initialize a new account and return it to JAVA side.<br>
* Since a C prt is returned as a jlong, special care will be taken
* to make the cast (OlmAccount* => jlong) platform independent.
* @return the initialized OlmAccount* instance
**/
JNIEXPORT jlong OLM_ACCOUNT_FUNC_DEF(initNewAccountJni)(JNIEnv *env, jobject thiz)
{
    const char* errorMessage = NULL;
    OlmAccount *accountPtr = initializeAccountMemory();

    // init account memory allocation
    if (!accountPtr)
    {
        LOGE("## initNewAccount(): failure - init account OOM");
        errorMessage = "init account OOM";
    }
    else
    {
        // get random buffer size
        size_t randomSize = olm_create_account_random_length(accountPtr);

        LOGD("## initNewAccount(): randomSize=%lu", static_cast<long unsigned int>(randomSize));

        uint8_t *randomBuffPtr = NULL;
        size_t accountRetCode;

        // allocate random buffer
        if ((0 != randomSize) && !setRandomInBuffer(env, &randomBuffPtr, randomSize))
        {
            LOGE("## initNewAccount(): failure - random buffer init");
            errorMessage = "random buffer init";
        }
        else
        {
            // create account
            accountRetCode = olm_create_account(accountPtr, (void*)randomBuffPtr, randomSize);

            if (accountRetCode == olm_error())
            {
                LOGE("## initNewAccount(): failure - account creation failed Msg=%s", olm_account_last_error(accountPtr));
                errorMessage = olm_account_last_error(accountPtr);
            }

            LOGD("## initNewAccount(): success - OLM account created");
            LOGD("## initNewAccount(): success - accountPtr=%p (jlong)(intptr_t)accountPtr=%lld",accountPtr,(jlong)(intptr_t)accountPtr);
        }

        if (randomBuffPtr)
        {
            free(randomBuffPtr);
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return (jlong)(intptr_t)accountPtr;
}

// *********************************************************************
// ************************* IDENTITY KEYS API *************************
// *********************************************************************
/**
* Get identity keys: Ed25519 fingerprint key and Curve25519 identity key.<br>
* The keys are returned in the byte array.
* @return a valid byte array if operation succeed, null otherwise
**/
JNIEXPORT jbyteArray OLM_ACCOUNT_FUNC_DEF(identityKeysJni)(JNIEnv *env, jobject thiz)
{
    const char* errorMessage = NULL;
    jbyteArray byteArrayRetValue = NULL;
    OlmAccount* accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);

    if (!accountPtr)
    {
        LOGE("## identityKeys(): failure - invalid Account ptr=NULL");
        errorMessage = "invalid Account ptr";
    }
    else
    {
        LOGD("## identityKeys(): accountPtr =%p", accountPtr);

        // identity keys allocation
        size_t identityKeysLength = olm_account_identity_keys_length(accountPtr);
        uint8_t *identityKeysBytesPtr = (uint8_t*)malloc(identityKeysLength);

        if (!identityKeysBytesPtr)
        {
            LOGE("## identityKeys(): failure - identity keys array OOM");
            errorMessage = "identity keys array OOM";
        }
        else
        {
            // retrieve key pairs in identityKeysBytesPtr
            size_t keysResult = olm_account_identity_keys(accountPtr, identityKeysBytesPtr, identityKeysLength);

            if (keysResult == olm_error())
            {
                errorMessage = (const char *)olm_account_last_error(accountPtr);
                LOGE("## identityKeys(): failure - error getting identity keys Msg=%s", errorMessage);
            }
            else
            {
                // allocate the byte array to be returned to java
                byteArrayRetValue = env->NewByteArray(identityKeysLength);

                if (!byteArrayRetValue)
                {
                    LOGE("## identityKeys(): failure - return byte array OOM");
                    errorMessage = "byte array OOM";
                }
                else
                {
                    env->SetByteArrayRegion(byteArrayRetValue, 0/*offset*/, identityKeysLength, (const jbyte*)identityKeysBytesPtr);
                    LOGD("## identityKeys(): success - result=%lu", static_cast<long unsigned int>(keysResult));
                }
            }

            free(identityKeysBytesPtr);
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return byteArrayRetValue;
}

// *********************************************************************
// ************************* ONE TIME KEYS API *************************
// *********************************************************************
/**
 * Get the maximum number of "one time keys" the account can store.
 *
**/
JNIEXPORT jlong OLM_ACCOUNT_FUNC_DEF(maxOneTimeKeysJni)(JNIEnv *env, jobject thiz)
{
    OlmAccount* accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);
    size_t maxKeys = -1;

    if (!accountPtr)
    {
        LOGE("## maxOneTimeKey(): failure - invalid Account ptr=NULL");
    }
    else
    {
        maxKeys = olm_account_max_number_of_one_time_keys(accountPtr);
    }

    LOGD("## maxOneTimeKey(): Max keys=%lu", static_cast<long unsigned int>(maxKeys));

    return (jlong)maxKeys;
}

/**
 * Generate "one time keys".
 * @param aNumberOfKeys number of keys to generate
**/
JNIEXPORT void OLM_ACCOUNT_FUNC_DEF(generateOneTimeKeysJni)(JNIEnv *env, jobject thiz, jint aNumberOfKeys)
{
    const char* errorMessage = NULL;
    OlmAccount *accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);

    if (!accountPtr)
    {
        LOGE("## generateOneTimeKeysJni(): failure - invalid Account ptr");
        errorMessage = "invalid Account ptr";
    }
    else
    {
        // keys memory allocation
        size_t randomLength = olm_account_generate_one_time_keys_random_length(accountPtr, (size_t)aNumberOfKeys);
        LOGD("## generateOneTimeKeysJni(): randomLength=%lu", static_cast<long unsigned int>(randomLength));

        uint8_t *randomBufferPtr = NULL;

        if ((0 != randomLength) && !setRandomInBuffer(env, &randomBufferPtr, randomLength))
        {
            LOGE("## generateOneTimeKeysJni(): failure - random buffer init");
            errorMessage = "random buffer init";
        }
        else
        {
            LOGD("## generateOneTimeKeysJni(): accountPtr =%p aNumberOfKeys=%d",accountPtr, aNumberOfKeys);

            // retrieve key pairs in keysBytesPtr
            size_t result = olm_account_generate_one_time_keys(accountPtr, (size_t)aNumberOfKeys, (void*)randomBufferPtr, randomLength);

            if (result == olm_error())
            {
                errorMessage = olm_account_last_error(accountPtr);
                LOGE("## generateOneTimeKeysJni(): failure - error generating one time keys Msg=%s", errorMessage);
            }
            else
            {
                LOGD("## generateOneTimeKeysJni(): success - result=%lu", static_cast<long unsigned int>(result));
            }
        }


        if (randomBufferPtr)
        {
            free(randomBufferPtr);
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }
}

/**
 * Get "one time keys".<br>
 * Return the public parts of the unpublished "one time keys" for the account
 * @return a valid byte array if operation succeed, null otherwise
**/
JNIEXPORT jbyteArray OLM_ACCOUNT_FUNC_DEF(oneTimeKeysJni)(JNIEnv *env, jobject thiz)
{
    const char* errorMessage = NULL;
    jbyteArray byteArrayRetValue = NULL;
    OlmAccount* accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);

    LOGD("## oneTimeKeysJni(): IN");

    if (!accountPtr)
    {
        LOGE("## oneTimeKeysJni(): failure - invalid Account ptr");
        errorMessage = "invalid Account ptr";
    }
    else
    {
        // keys memory allocation
        size_t keysLength = olm_account_one_time_keys_length(accountPtr);
        uint8_t *keysBytesPtr = (uint8_t *)malloc(keysLength*sizeof(uint8_t));

        if (!keysBytesPtr)
        {
            LOGE("## oneTimeKeysJni(): failure - one time keys array OOM");
            errorMessage = "one time keys array OOM";
        }
        else
        {
            // retrieve key pairs in keysBytesPtr
            size_t keysResult = olm_account_one_time_keys(accountPtr, keysBytesPtr, keysLength);

            if (keysResult == olm_error()) {
                LOGE("## oneTimeKeysJni(): failure - error getting one time keys Msg=%s",(const char *)olm_account_last_error(accountPtr));
                errorMessage = (const char *)olm_account_last_error(accountPtr);
            }
            else
            {
                // allocate the byte array to be returned to java
                byteArrayRetValue = env->NewByteArray(keysLength);

                if (!byteArrayRetValue)
                {
                    LOGE("## oneTimeKeysJni(): failure - return byte array OOM");
                    errorMessage = "return byte array OOM";
                }
                else
                {
                    env->SetByteArrayRegion(byteArrayRetValue, 0/*offset*/, keysLength, (const jbyte*)keysBytesPtr);
                    LOGD("## oneTimeKeysJni(): success");
                }
            }

            free(keysBytesPtr);
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return byteArrayRetValue;
}

/**
 * Remove the "one time keys"  that the session used from the account.
 * Return the public parts of the unpublished "one time keys" for the account
 * @param aNativeOlmSessionId session instance
 * @return ERROR_CODE_OK if operation succeed, ERROR_CODE_KO otherwise
**/
JNIEXPORT jint OLM_ACCOUNT_FUNC_DEF(removeOneTimeKeysJni)(JNIEnv *env, jobject thiz, jlong aNativeOlmSessionId)
{
    const char* errorMessage = NULL;
    jint retCode = ERROR_CODE_KO;
    OlmAccount* accountPtr = NULL;
    OlmSession* sessionPtr = (OlmSession*)aNativeOlmSessionId;

    if (!sessionPtr)
    {
        LOGE("## removeOneTimeKeysJni(): failure - invalid session ptr");
        errorMessage = "invalid session ptr";
    }
    else if(!(accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz)))
    {
        LOGE("## removeOneTimeKeysJni(): failure - invalid account ptr");
        errorMessage = "invalid account ptr";
    }
    else
    {
        size_t result = olm_remove_one_time_keys(accountPtr, sessionPtr);

        if (result == olm_error())
        {   // the account doesn't have any matching "one time keys"..
            LOGW("## removeOneTimeKeysJni(): failure - removing one time keys Msg=%s",(const char *)olm_account_last_error(accountPtr));
            errorMessage = (const char *)olm_account_last_error(accountPtr);
        }
        else
        {
            retCode = ERROR_CODE_OK;
            LOGD("## removeOneTimeKeysJni(): success");
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return retCode;
}

/**
 * Mark the current set of "one time keys" as being published.
**/
JNIEXPORT void OLM_ACCOUNT_FUNC_DEF(markOneTimeKeysAsPublishedJni)(JNIEnv *env, jobject thiz)
{
    const char* errorMessage = NULL;
    OlmAccount* accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz);

    if (!accountPtr)
    {
        LOGE("## markOneTimeKeysAsPublishedJni(): failure - invalid account ptr");
        errorMessage = "invalid account ptr";
    }
    else
    {
        size_t result = olm_account_mark_keys_as_published(accountPtr);

        if (result == olm_error())
        {
            LOGW("## markOneTimeKeysAsPublishedJni(): failure - Msg=%s",(const char *)olm_account_last_error(accountPtr));
            errorMessage = (const char *)olm_account_last_error(accountPtr);
        }
        else
        {
            LOGD("## markOneTimeKeysAsPublishedJni(): success - retCode=%lu",static_cast<long unsigned int>(result));
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }
}

/**
 * Sign a message with the ed25519 key (fingerprint) for this account.<br>
 * The signed message is returned by the function.
 * @param aMessage message to sign
 * @return the signed message, null otherwise
**/
JNIEXPORT jbyteArray OLM_ACCOUNT_FUNC_DEF(signMessageJni)(JNIEnv *env, jobject thiz, jbyteArray aMessage)
{
    const char* errorMessage = NULL;
    OlmAccount* accountPtr = NULL;
    jbyteArray signedMsgRetValueBuffer = NULL;

    if (!aMessage)
    {
        LOGE("## signMessageJni(): failure - invalid aMessage param");
        errorMessage = "invalid aMessage param";
    }
    else if(!(accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz)))
    {
        LOGE("## signMessageJni(): failure - invalid account ptr");
        errorMessage = "invalid account ptr";
    }
    else
    {
        int messageLength = env->GetArrayLength(aMessage);
        jbyte* messageToSign = env->GetByteArrayElements(aMessage, NULL);

        // signature memory allocation
        size_t signatureLength = olm_account_signature_length(accountPtr);
        size_t bufferLen = signatureLength + 1;
        void* signedMsgPtr = malloc(bufferLen * sizeof(uint8_t));

        if (!signedMsgPtr)
        {
            LOGE("## signMessageJni(): failure - signature allocation OOM");
            errorMessage = "signature allocation OOM";
        }
        else
        {
            // sign message
            size_t resultSign = olm_account_sign(accountPtr,
                                   (void*)messageToSign,
                                   (size_t)messageLength,
                                   signedMsgPtr,
                                   signatureLength);

            if (resultSign == olm_error())
            {
                LOGE("## signMessageJni(): failure - error signing message Msg=%s",(const char *)olm_account_last_error(accountPtr));
                errorMessage = (const char *)olm_account_last_error(accountPtr);
            }
            else
            {
                // info: signatureLength is always equal to resultSign
                (static_cast<char*>(signedMsgPtr))[signatureLength] = static_cast<char>('\0');

                LOGD("## signMessageJni(): success - retCode=%lu signatureLength=%lu", static_cast<long unsigned int>(resultSign), static_cast<long unsigned int>(signatureLength));
     	
                signedMsgRetValueBuffer = env->NewByteArray(signatureLength);
                env->SetByteArrayRegion(signedMsgRetValueBuffer, 0 , signatureLength, (jbyte*)signedMsgPtr);

            }

            free(signedMsgPtr);
        }

        // release messageToSign
        if (messageToSign)
        {
            env->ReleaseByteArrayElements(aMessage, messageToSign, JNI_ABORT);
        }
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return signedMsgRetValueBuffer;
}

/**
* Serialize and encrypt account instance into a base64 string.<br>
* @param aKeyBuffer key used to encrypt the serialized account data
* @return a base64 string if operation succeed, null otherwise
**/
JNIEXPORT jbyteArray OLM_ACCOUNT_FUNC_DEF(serializeJni)(JNIEnv *env, jobject thiz, jbyteArray aKeyBuffer)
{
    const char* errorMessage = NULL;
    jbyteArray pickledDataRetValue = 0;
    jbyte* keyPtr = NULL;
    OlmAccount* accountPtr = NULL;

    LOGD("## serializeJni(): IN");

    if (!aKeyBuffer)
    {
        LOGE(" ## serializeJni(): failure - invalid key");
        errorMessage = "invalid key";
    }
    else if (!(accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz)))
    {
       LOGE(" ## serializeJni(): failure - invalid account ptr");
       errorMessage = "invalid account ptr";
    }
    else if (!(keyPtr = env->GetByteArrayElements(aKeyBuffer, NULL)))
    {
        LOGE(" ## serializeJni(): failure - keyPtr JNI allocation OOM");
        errorMessage = "keyPtr JNI allocation OOM";
    }
    else
    {
        size_t pickledLength = olm_pickle_account_length(accountPtr);
        size_t keyLength = (size_t)env->GetArrayLength(aKeyBuffer);
        LOGD(" ## serializeJni(): pickledLength=%lu keyLength=%lu",static_cast<long unsigned int>(pickledLength), static_cast<long unsigned int>(keyLength));
        LOGD(" ## serializeJni(): key=%s",(char const *)keyPtr);

        void *pickledPtr = malloc((pickledLength+1)*sizeof(uint8_t));

        if (!pickledPtr)
        {
            LOGE(" ## serializeJni(): failure - pickledPtr buffer OOM");
            errorMessage = "pickledPtr buffer OOM";
        }
        else
        {
            size_t result = olm_pickle_account(accountPtr,
                                               (void const *)keyPtr,
                                               keyLength,
                                               (void*)pickledPtr,
                                               pickledLength);
            if (result == olm_error())
            {
                errorMessage = olm_account_last_error(accountPtr);
                LOGE(" ## serializeJni(): failure - olm_pickle_account() Msg=%s", errorMessage);
            }
            else
            {
                // build success output
                (static_cast<char*>(pickledPtr))[pickledLength] = static_cast<char>('\0');

                LOGD(" ## serializeJni(): success - result=%lu pickled=%s", static_cast<long unsigned int>(result), static_cast<char*>(pickledPtr));

                pickledDataRetValue = env->NewByteArray(pickledLength+1);
                env->SetByteArrayRegion(pickledDataRetValue, 0 , pickledLength+1, (jbyte*)pickledPtr);
            }

            free(pickledPtr);
        }
    }

    // free alloc
    if (keyPtr)
    {
        env->ReleaseByteArrayElements(aKeyBuffer, keyPtr, JNI_ABORT);
    }

    if (errorMessage)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), errorMessage);
    }

    return pickledDataRetValue;
}


JNIEXPORT jstring OLM_ACCOUNT_FUNC_DEF(deserializeJni)(JNIEnv *env, jobject thiz, jbyteArray aSerializedDataBuffer, jbyteArray aKeyBuffer)
{
    OlmAccount* accountPtr = NULL;
    jstring errorMessageRetValue = 0;
    jbyte* keyPtr = NULL;
    jbyte* pickledPtr = NULL;

    LOGD("## deserializeJni(): IN");

    if (!aKeyBuffer)
    {
        LOGE(" ## deserializeJni(): failure - invalid key");
    }
    else if (!aSerializedDataBuffer)
    {
        LOGE(" ## deserializeJni(): failure - serialized data");
    }
    else if (!(accountPtr = (OlmAccount*)getAccountInstanceId(env,thiz)))
    {
        LOGE(" ## deserializeJni(): failure - account failure OOM");
    }
    else if (!(keyPtr = env->GetByteArrayElements(aKeyBuffer, 0)))
    {
        LOGE(" ## deserializeJni(): failure - keyPtr JNI allocation OOM");
    }
    else if (!(pickledPtr = env->GetByteArrayElements(aSerializedDataBuffer, 0)))
    {
        LOGE(" ## deserializeJni(): failure - pickledPtr JNI allocation OOM");
    }
    else
    {
        size_t pickledLength = (size_t)env->GetArrayLength(aSerializedDataBuffer);
        size_t keyLength = (size_t)env->GetArrayLength(aKeyBuffer);
        LOGD(" ## deserializeJni(): pickledLength=%lu keyLength=%lu",static_cast<long unsigned int>(pickledLength), static_cast<long unsigned int>(keyLength));
        LOGD(" ## deserializeJni(): key=%s",(char const *)keyPtr);
        LOGD(" ## deserializeJni(): pickled=%s",(char const *)pickledPtr);

        size_t result = olm_unpickle_account(accountPtr,
                                             (void const *)keyPtr,
                                             keyLength,
                                             (void*)pickledPtr,
                                             pickledLength);
        if (result == olm_error())
        {
            const char *errorMsgPtr = olm_account_last_error(accountPtr);
            LOGE(" ## deserializeJni(): failure - olm_unpickle_account() Msg=%s",errorMsgPtr);
            errorMessageRetValue = env->NewStringUTF(errorMsgPtr);
        }
        else
        {
            LOGD(" ## deserializeJni(): success - result=%lu ", static_cast<long unsigned int>(result));
        }
    }

    // free alloc
    if (keyPtr)
    {
        env->ReleaseByteArrayElements(aKeyBuffer, keyPtr, JNI_ABORT);
    }

    if (pickledPtr)
    {
        env->ReleaseByteArrayElements(aSerializedDataBuffer, pickledPtr, JNI_ABORT);
    }

    return errorMessageRetValue;
}