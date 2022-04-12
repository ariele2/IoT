from flask import Flask, render_template, Response, request, redirect, url_for
import firebase_admin
from firebase_admin import db

app = Flask(__name__)

cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'})
ref = db.reference("/test/action")

def updateDB():
    data = ref.get()
    if data == "stop":
        ref.set("start")
        return "start"
    ref.set("stop")
    return "stop"

@app.route("/")  # this sets the route to this page
def home():
    data = ref.get()
    return render_template("index.html", val=data)

@app.route("/updateDB")
def start_db():
    res = updateDB()
    return redirect(url_for("home"))
    # return render_template("index.html", val=res)

if __name__ == "__main__":
    app.run()