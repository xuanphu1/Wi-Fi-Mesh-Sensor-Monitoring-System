import zlib
import base64
import urllib.request
import string
import os

def encode(puml_text):
    zlibbed_str = zlib.compress(puml_text.encode('utf-8'))
    compressed_string = zlibbed_str[2:-4]
    
    b64_str = base64.b64encode(compressed_string).decode('utf-8')
    
    b64_table = string.ascii_uppercase + string.ascii_lowercase + string.digits + '+/'
    puml_table = string.digits + string.ascii_uppercase + string.ascii_lowercase + '-_'
    
    trans = str.maketrans(b64_table, puml_table)
    return b64_str.translate(trans)

def download(puml_file, out_png):
    with open(puml_file, 'r', encoding='utf-8') as f:
        text = f.read()
    encoded = encode(text)
    url = f"http://www.plantuml.com/plantuml/png/{encoded}"
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req) as response, open(out_png, 'wb') as out_file:
        out_file.write(response.read())
    print(f"Downloaded {out_png}")

os.makedirs('pic', exist_ok=True)
download('plantuml_diagrams/data_flow.puml', 'pic/puml_data_flow.png')
download('plantuml_diagrams/frontend_sequence.puml', 'pic/puml_frontend.png')
download('plantuml_diagrams/folder_sequence.puml', 'pic/puml_folder_sequence.png')
download('plantuml_diagrams/directory_tree.puml', 'pic/puml_tree.png')
download('plantuml_diagrams/architecture.puml', 'pic/puml_architecture.png')
download('plantuml_diagrams/seq_connect.puml', 'pic/puml_seq_connect.png')
download('plantuml_diagrams/seq_realtime.puml', 'pic/puml_seq_realtime.png')
download('plantuml_diagrams/seq_history.puml', 'pic/puml_seq_history.png')
download('plantuml_diagrams/seq_fota.puml', 'pic/puml_seq_fota.png')
download('plantuml_diagrams/tree_components.puml', 'pic/puml_tree_components.png')
download('plantuml_diagrams/tree_store.puml', 'pic/puml_tree_store.png')
download('plantuml_diagrams/tree_services.puml', 'pic/puml_tree_services.png')
download('plantuml_diagrams/tree_others.puml', 'pic/puml_tree_others.png')
