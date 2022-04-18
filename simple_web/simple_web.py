from flask import Flask, redirect, url_for, render_template, session
from flask_wtf import FlaskForm
from wtforms.fields import DateField
from wtforms.validators import DataRequired
from wtforms import validators, SubmitField
import firebase_admin
from firebase_admin import db, credentials, initialize_app, storage
import pandas as pd

app = Flask(__name__)

app.config['SECRET_KEY'] = '#$%^&*'

cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
storage_path = 'iotprojdb.appspot.com'
firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path, 'storageBucket': storage_path})

ref = db.reference("/test/action")
bucket = storage.bucket()
# blob = bucket.blob(fileName)
# blob.upload_from_filename(fileName)

class InfoForm(FlaskForm):
    startdate = DateField('', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    enddate = DateField('-', format='%Y-%m-%d', validators=(validators.DataRequired(),))
    submit = SubmitField('Generate CSV')

def updateDB():
    data = ref.get()
    if data == "stop":
        ref.set("start")
        return "start"
    ref.set("stop")
    return "stop"

def generateCSV(start_date, end_date):
    # create the columns titles
    pass
    # query the firebase on the given dates
    # store all of the data in a pandas table to generate a .csv file 

@app.route("/", methods=['GET','POST'])  # this sets the route to this page
def home():
    form = InfoForm()
    data = ref.get()
    if form.validate_on_submit():
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        return redirect('generateCSV')
    return render_template("index.html", val=data, form=form)


@app.route("/updateDB")
def update_db():
    res = updateDB()
    return redirect(url_for("home"))


@app.route("/generateCSV")
def generate_csv():
    startdate = session['startdate']
    enddate = session['enddate']
    res = generateCSV(startdate, enddate)
    return redirect(url_for("home"))


if __name__ == "__main__":
    app.run()