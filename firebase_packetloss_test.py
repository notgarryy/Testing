import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore
import time
import uuid 

SERVICE_ACCOUNT_KEY_PATH = "./CD/serviceAccountKey.json" 
TEST_COLLECTION_NAME = "firebase_packetloss_test" 

try:
    cred = credentials.Certificate(SERVICE_ACCOUNT_KEY_PATH)
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    print("Firebase Admin SDK berhasil diinisialisasi untuk Firestore.")
except Exception as e:
    print(f"Gagal menginisialisasi Firebase Admin SDK: {e}")
    exit()
    
def send_data_to_firestore(collection_name, data, doc_id=None):
    try:
        if doc_id:
            doc_ref = db.collection(collection_name).document(doc_id)
        else:
            doc_ref = db.collection(collection_name).document() 

        data_with_timestamp = data.copy() 
        data_with_timestamp['server_timestamp'] = firestore.SERVER_TIMESTAMP 

        doc_ref.set(data_with_timestamp)
        print(f"Data berhasil dikirim ke Firestore. ID Dokumen: {doc_ref.id}")
        return True, doc_ref.id 
    except Exception as e:
        print(f"Gagal mengirim data ke Firestore: {e}")
        return False, None

def verify_document_in_firestore(collection_name, doc_id):
    try:
        doc_ref = db.collection(collection_name).document(doc_id)
        doc = doc_ref.get()
        if doc.exists:
            print(f"Dokumen {doc_id} ditemukan: {doc.to_dict()}")
            return True
        else:
            print(f"Dokumen {doc_id} tidak ditemukan.")
            return False
    except Exception as e:
        print(f"Error saat memverifikasi dokumen {doc_id}: {e}")
        return False
    
def delete_document_from_firestore(collection_name, doc_id):
    try:
        db.collection(collection_name).document(doc_id).delete()
        print(f"Dokumen {doc_id} berhasil dihapus dari koleksi {collection_name}.")
    except Exception as e:
        print(f"Gagal menghapus dokumen {doc_id}: {e}")
        
def test_packet_loss_firestore():
    print(f"Memulai pengujian kehilangan paket ke Firestore dengan {TOTAL_PACKETS_TO_SEND} dokumen...")
    packets_sent_successfully = 0
    packets_verified_on_server = 0
    sent_document_ids = []

    for i in range(TOTAL_PACKETS_TO_SEND):
        client_doc_id = f"packet_{uuid.uuid4()}" 
        data_to_send = {
            "sequence": i + 1,
            "message": f"Tes dokumen ke-{i+1}",
            "client_timestamp": time.time() 
        }
        
        success, doc_id = send_data_to_firestore(TEST_COLLECTION_NAME, data_to_send)
        
        if success and doc_id:
            packets_sent_successfully += 1
            sent_document_ids.append(doc_id)
        time.sleep(0.1) 

    print(f"\nVerifikasi dokumen di Firestore...")
    for doc_id_to_verify in sent_document_ids:
        if verify_document_in_firestore(TEST_COLLECTION_NAME, doc_id_to_verify):
            packets_verified_on_server +=1
        time.sleep(0.05)

    print(f"\n--- Hasil Pengujian Kehilangan Paket (Firestore) ---")
    print(f"Total dokumen yang seharusnya dikirim: {TOTAL_PACKETS_TO_SEND}")
    print(f"Total dokumen berhasil dikirim (berdasarkan SDK): {packets_sent_successfully}")
    print(f"Total dokumen terverifikasi di Firestore: {packets_verified_on_server}")

    if TOTAL_PACKETS_TO_SEND > 0:
        loss_percentage = ((TOTAL_PACKETS_TO_SEND - packets_verified_on_server) / TOTAL_PACKETS_TO_SEND) * 100
        print(f"Kehilangan Paket (Firestore): {loss_percentage:.2f}%")
    else:
        print("Tidak ada dokumen yang dikirim.")

if __name__ == "__main__":
    TOTAL_PACKETS_TO_SEND = 1000
    test_packet_loss_firestore()