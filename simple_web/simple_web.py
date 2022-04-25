from flask import Flask, redirect, url_for, render_template, session
from flask_wtf import FlaskForm
from wtforms.fields import DateField
from wtforms.validators import DataRequired
from wtforms import validators, SubmitField
import firebase_admin
from firebase_admin import db, credentials, initialize_app, storage
import pandas as pd
import datetime

app = Flask(__name__)

app.config['SECRET_KEY'] = '#$%^&*'

cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
storage_path = 'iotprojdb.appspot.com'
firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path, 'storageBucket': storage_path})

csv_path = "csv_reports/"
ref = db.reference("/test/action")
bucket = storage.bucket()


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

def date2num(date):
    list_date = date.split(' ')
    day = int(list_date[1])
    month = datetime.datetime.strptime(list_date[2], "%b").month
    year = int(list_date[3])
    return [day,month,year]


def validateDates(start_date, end_date):
    if end_date[2] - start_date[2] < 0:     # end year is smaller then start year
        return False
    elif end_date[2] - start_date[2] > 0:
        return True
    else:   # the same year
        if end_date[1] - start_date[1] < 0:
            return False
        elif end_date[1] - start_date[1] > 0:
            return True
        else:   # the same month
            if end_date[0] - start_date[0] < 0:
                return False
            else:   # the same day or greater
                return True

    return False    # shouldn't reach here


def generateCSV(start_date, end_date):
    # create the columns titles
    columns_titles = ["sensorID", "callID", "time", "value"]
    df = pd.DataFrame(columns=columns_titles)
    # transfer date value to readable integers
    start_date = date2num(start_date)
    end_date = date2num(end_date)
    # validate the dates
    if not validateDates(start_date, end_date):
        return "ERROR"
    # query the dates and arrange in the dataframe
    # create the csv file
    start_date_str = str(start_date[0]) + '.' + str(start_date[1]) + '.' + str(start_date[2])
    end_date_str = str(end_date[0]) + '.' + str(end_date[1]) + '.' + str(end_date[2])
    new_csv_filename = csv_path + "report" + start_date_str + "-" + end_date_str + ".csv"
    df.to_csv(new_csv_filename) 
    # upload the file to the storage
    blob = bucket.blob(new_csv_filename)
    blob.upload_from_filename(new_csv_filename)
    blob.make_public()
    # generate a download url and return it
    return blob.public_url


@app.route("/", methods=['GET','POST'])  # this sets the route to this page
def home():
    form = InfoForm()
    data = ref.get()
    res = session['res'] if 'res' in session else ''
    if res == 'Please enter a valid date range':
        session.pop('res')
        res = 'Please enter a valid date range'
    if form.validate_on_submit():
        session['startdate'] = form.startdate.data
        session['enddate'] = form.enddate.data
        return redirect('generateCSV')
    return render_template("index.html", val=data, form=form, res=res)


@app.route("/updateDB")
def update_db():
    res = updateDB()
    return redirect(url_for("home"))


@app.route("/generateCSV")
def generate_csv():
    startdate = session['startdate']
    enddate = session['enddate']
    err_message = 'Please enter a valid date range'
    res = generateCSV(startdate, enddate)
    print('res: ', res)
    if res == "ERROR":
        session['res'] = err_message
        return redirect(url_for("home", res=err_message))
    else:
        session['res'] = res
        return redirect(url_for("home", res=res))

@app.route("/status")
def getStatus():
    # add the query on the database for the status
    return render_template("status.html")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=1234)