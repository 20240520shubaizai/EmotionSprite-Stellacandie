import argparse
import json
import os
from pathlib import Path
import uvicorn
from agent_core.app import app
from agent_core.data_lifecycle import delete_data,export_data,restore_data,default_data_directory

parser=argparse.ArgumentParser()
parser.add_argument("--host",default="127.0.0.1")
parser.add_argument("--port",type=int)
parser.add_argument("--token",default=os.getenv("AGENT_SESSION_TOKEN",""))
parser.add_argument("--data-dir",type=Path,default=default_data_directory())
parser.add_argument("--export-data",type=Path)
parser.add_argument("--restore-data",type=Path)
parser.add_argument("--delete-data",action="store_true")
parser.add_argument("--confirmation",default="")
parser.add_argument("--delete-credentials",action="store_true")
args=parser.parse_args()
if args.export_data:
    print(json.dumps(export_data(args.data_dir,args.export_data),ensure_ascii=False));raise SystemExit(0)
if args.restore_data:
    print(json.dumps(restore_data(args.restore_data,args.data_dir),ensure_ascii=False));raise SystemExit(0)
if args.delete_data:
    print(json.dumps(delete_data(args.data_dir,args.confirmation,delete_credentials=args.delete_credentials),ensure_ascii=False));raise SystemExit(0)
if args.port is None:
    parser.error("--port is required unless a data maintenance operation is selected")
if not args.token:
    parser.error("session token is required")
os.environ["AGENT_SESSION_TOKEN"]=args.token
uvicorn.run(app,host=args.host,port=args.port,log_config=None,access_log=False)
