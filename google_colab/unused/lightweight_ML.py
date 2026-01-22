import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score
from sklearn.utils.multiclass import unique_labels
import joblib

def main():
    df = pd.read_csv('labeled_dataset.csv')
    label_counts = df['label'].value_counts()
    valid_labels = label_counts[label_counts >= 2].index
    
    df = df[df['label'].isin(valid_labels)]
    
    exclude_cols = ['label', 'anomaly_prediction','anomaly_score','is_anomaly']
    feature_cols = [col for col in df.columns if col not in exclude_cols]

    x = df[feature_cols]
    y = df['label']

    #Encode labels
    label_encoder = LabelEncoder()
    y_encoded = label_encoder.fit_transform(y)

    #Split data
    x_train, x_test, y_train, y_test = train_test_split(
        x, y_encoded, test_size=0.2, random_state=42, stratify=y_encoded
    )

    #Scale features
    scaler = StandardScaler()
    x_train_scaled = scaler.fit_transform(x_train)
    x_test_scaled = scaler.transform(x_test)

    #Train Random Forest
    rf_classifier = RandomForestClassifier(
        n_estimators = 30,
        max_depth = 8,
        min_samples_split = 5,
        min_samples_leaf = 3,
        max_features = 'sqrt',
        random_state = 42,
        n_jobs = -1
    )

    rf_classifier.fit(x_train_scaled,y_train)

    #Evaluate
    y_pred = rf_classifier.predict(x_test_scaled)
    accuracy = accuracy_score(y_test,y_pred)

    labels_used = unique_labels(y_test, y_pred)
    target_names_used = label_encoder.inverse_transform(labels_used)
    
    print(f'Accuracy: {accuracy: .4f}')
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=target_names_used))

    #Save Models
    joblib.dump(rf_classifier, 'failure_classifier_rf.pkl')
    joblib.dump(scaler, 'failure_scaler.pkl')
    joblib.dump(label_encoder, 'label_encoder.pkl')

    #Save feature list
    with open('model_features.txt', 'w') as f:
        for feature in feature_cols:
            f.write(f"{feature}\n")
    
    print("Random Forest training complete. Models saved.")

if __name__ == '__main__':
    main()