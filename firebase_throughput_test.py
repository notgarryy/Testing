import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore
import time

SERVICE_ACCOUNT_KEY_PATH = "./CD/serviceAccountKey.json"

db = None 

def initialize_firebase():
    """Menginisialisasi Firebase Admin SDK."""
    global db
    try:
        if not firebase_admin._apps:
            cred = credentials.Certificate(SERVICE_ACCOUNT_KEY_PATH)
            firebase_admin.initialize_app(cred)
        db = firestore.client()
        print("Firebase Admin SDK berhasil diinisialisasi untuk Firestore.")
        return True
    except Exception as e:
        print(f"Gagal menginisialisasi Firebase Admin SDK: {e}")
        return False

if not initialize_firebase():
    exit()

THROUGHPUT_COLLECTION_NAME = "firebase_throughput_test" 
TEST_DURATION_SECONDS = 1000 

def send_data_for_throughput_firestore(data):
    """Mengirim satu unit data ke Firestore dan mengembalikan status serta ukuran."""
    try:
        doc_ref = db.collection(THROUGHPUT_COLLECTION_NAME).document()
        
        data_with_timestamp = data.copy()
        data_with_timestamp['server_timestamp'] = firestore.SERVER_TIMESTAMP
        
        doc_ref.set(data_with_timestamp)
        return True, 1 
    except Exception as e:
        print(f"Gagal mengirim data throughput: {e}")
        return False, 0

def test_firestore_throughput():
    print(f"Memulai pengujian throughput ke Firestore selama {TEST_DURATION_SECONDS} detik...")
    start_time = time.time()
    successful_data_points = 0

    print(f"Membersihkan koleksi pengujian: {THROUGHPUT_COLLECTION_NAME} (jika ada)...")
    docs = db.collection(THROUGHPUT_COLLECTION_NAME).limit(1000).stream()
    deleted = 0
    for doc in docs:
        doc.reference.delete()
        deleted += 1
    if deleted > 0:
        print(f"{deleted} dokumen lama dihapus.")

    while (time.time() - start_time) < TEST_DURATION_SECONDS:
        data_packet = {
            "value": time.time() % 100, 
            "client_timestamp": time.time()
        }
        success, data_units_sent = send_data_for_throughput_firestore(data_packet)
        if success:
            successful_data_points += data_units_sent

    end_time = time.time()
    actual_duration = end_time - start_time

    print(f"\n--- Hasil Pengujian Throughput (Firestore) ---")
    print(f"Durasi pengujian aktual: {actual_duration:.2f} detik")
    print(f"Total data point berhasil dikirim: {successful_data_points}")

    if actual_duration > 0:
        points_per_second = successful_data_points / actual_duration
        print(f"Throughput (data points/detik): {points_per_second:.2f}")
    else:
        print("Durasi pengujian terlalu singkat.")

if __name__ == "__main__":
    if db:
        test_firestore_throughput()